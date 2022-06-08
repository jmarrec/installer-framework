/**************************************************************************
**
** Copyright (C) 2022 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Installer Framework.
**
** $QT_BEGIN_LICENSE:GPL-EXCEPT$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
**************************************************************************/

#include "installercalculator.h"

#include "component.h"
#include "packagemanagercore.h"
#include "settings.h"
#include <globals.h>

namespace QInstaller {

/*!
    \inmodule QtInstallerFramework
    \class QInstaller::InstallerCalculator
    \internal
*/

InstallerCalculator::InstallerCalculator(PackageManagerCore *core, const QList<Component *> &allComponents, const AutoDependencyHash &autoDependencyComponentHash)
    : m_core(core)
    , m_allComponents(allComponents)
    , m_autoDependencyComponentHash(autoDependencyComponentHash)
{
}

InstallerCalculator::InstallReasonType InstallerCalculator::installReasonType(const Component *c) const
{
    return m_toInstallComponentIdReasonHash.value(c->name()).first;
}

QString InstallerCalculator::installReason(const Component *component) const
{
    InstallerCalculator::InstallReasonType reason = installReasonType(component);
    switch (reason) {
        case Automatic:
            return QCoreApplication::translate("InstallerCalculator",
                "Components added as automatic dependencies:");
        case Dependent:
            return QCoreApplication::translate("InstallerCalculator", "Components added as "
                "dependency for \"%1\":").arg(installReasonReferencedComponent(component));
        case Resolved:
            return QCoreApplication::translate("InstallerCalculator",
                "Components that have resolved dependencies:");
        case Selected:
            return QCoreApplication::translate("InstallerCalculator",
                "Selected components without dependencies:");
    }
    return QString();
}

QList<Component*> InstallerCalculator::orderedComponentsToInstall() const
{
    return m_orderedComponentsToInstall;
}

QString InstallerCalculator::componentsToInstallError() const
{
    return m_componentsToInstallError;
}

bool InstallerCalculator::appendComponentsToInstall(const QList<Component *> &components, bool modelReset, const bool revertFromInstall)
{
    if (components.isEmpty())
        return true;

    QList<Component*> notAppendedComponents; // for example components with unresolved dependencies
    foreach (Component *component, components) {
        if (!component)
            continue;
        // When model has been reseted, there should not be components with the same name
        if (m_toInstallComponentIds.contains(component->name())) {
            if (modelReset) {
                const QString errorMessage = recursionError(component);
                qCWarning(QInstaller::lcInstallerInstallLog).noquote() << errorMessage;
                m_componentsToInstallError.append(errorMessage);
                Q_ASSERT_X(!m_toInstallComponentIds.contains(component->name()), Q_FUNC_INFO,
                    qPrintable(errorMessage));
                return false;
            }
            if (!revertFromInstall) {
              // We can end up here if component is already added as dependency and
              // user explicitly selects it to install
              continue;
            }
        }

        if (component->currentDependencies().isEmpty())
            realAppendToInstallComponents(component, QString(), revertFromInstall);
        else
            notAppendedComponents.append(component);
    }

    foreach (Component *component, notAppendedComponents) {
        if (!appendComponentToInstall(component, QString(), revertFromInstall))
            return false;
    }

    // All regular dependencies are resolved. Now we are looking for auto depend on components.
    QSet<Component *> foundAutoDependOnList = autodependencyComponents(revertFromInstall);

    if (!foundAutoDependOnList.isEmpty())
        return appendComponentsToInstall(foundAutoDependOnList.values(), modelReset, revertFromInstall);

    return true;
}

bool InstallerCalculator::removeComponentsFromInstall(const QList<Component *> &components)
{
    return appendComponentsToInstall(components, false, true);
}

QString InstallerCalculator::installReasonReferencedComponent(const Component *component) const
{
   const QString componentName = component->name();
   if (m_referenceCount.contains(componentName))
       return m_referenceCount.value(componentName).first();
   return m_toInstallComponentIdReasonHash.value(componentName,
           qMakePair(InstallerCalculator::Selected, QString())).second;
}

void InstallerCalculator::insertInstallReason(const Component *component,
        const InstallReasonType installReason, const QString &referencedComponentName, const bool revertFromInstall)
{
    if (revertFromInstall && m_toInstallComponentIdReasonHash.contains(component->name())) {
        m_toInstallComponentIdReasonHash.remove(component->name());
    } else if (!m_toInstallComponentIdReasonHash.contains(component->name())) {
        m_toInstallComponentIdReasonHash.insert(component->name(),
                qMakePair(installReason, referencedComponentName));
    }
}

void InstallerCalculator::realAppendToInstallComponents(Component *component, const QString &version, const bool revertFromInstall)
{
    if (!m_componentsForAutodepencencyCheck.contains(component))
        m_componentsForAutodepencencyCheck.append(component);
    if (revertFromInstall) {
        if (m_toInstallComponentIds.contains(component->name())) {
            // Component is no longer added as dependency and is not explicitly selected for install by user
            if (m_referenceCount.value(component->name()).isEmpty() && !component->isSelected()) {
                m_toInstallComponentIds.remove(component->name());
                m_orderedComponentsToInstall.removeAll(component);
            }
        }
    } else {
        if (!component->isInstalled(version)
                || (m_core->isUpdater() && component->isUpdateAvailable())) {
            m_toInstallComponentIds.insert(component->name());
            m_orderedComponentsToInstall.append(component);
        }
    }
}

bool InstallerCalculator::appendComponentToInstall(Component *component, const QString &version, bool revertFromInstall)
{
    const QStringList dependenciesList = component->currentDependencies();
    QSet<QString> allDependencies(dependenciesList.begin(), dependenciesList.end());
    QString requiredDependencyVersion = version;
    foreach (const QString &dependencyComponentName, allDependencies) {
        // PackageManagerCore::componentByName returns 0 if dependencyComponentName contains a
        // version which is not available
        Component *dependencyComponent =
            PackageManagerCore::componentByName(dependencyComponentName, m_allComponents);
        if (!dependencyComponent) {
            const QString errorMessage = QCoreApplication::translate("InstallerCalculator",
                "Cannot find missing dependency \"%1\" for \"%2\".").arg(dependencyComponentName,
                component->name());
            qCWarning(QInstaller::lcInstallerInstallLog).noquote() << errorMessage;
            m_componentsToInstallError.append(errorMessage);
            if (component->packageManagerCore()->settings().allowUnstableComponents()) {
                component->setUnstable(Component::UnstableError::MissingDependency, errorMessage);
                continue;
            } else {
                return false;
            }
        }
        if (revertFromInstall && dependencyComponent->forcedInstallation())
            continue;
        //Check if component requires higher version than what might be already installed
        bool isUpdateRequired = false;
        QString requiredName;
        QString requiredVersion;
        PackageManagerCore::parseNameAndVersion(dependencyComponentName, &requiredName, &requiredVersion);
        if (!requiredVersion.isEmpty() &&
                !dependencyComponent->value(scInstalledVersion).isEmpty()) {
            QRegularExpression compEx(QRegularExpression::anchoredPattern(QLatin1String("([<=>]+)(.*)")));
            QRegularExpressionMatch match = compEx.match(dependencyComponent->value(scInstalledVersion));
            const QString installedVersion = match.hasMatch() ? compEx.captured(2) : dependencyComponent->value(scInstalledVersion);

            match = compEx.match(requiredVersion);
            requiredVersion = match.hasMatch() ? compEx.captured(2) : requiredVersion;

            if (KDUpdater::compareVersion(requiredVersion, installedVersion) >= 1 ) {
                isUpdateRequired = true;
                requiredDependencyVersion = requiredVersion;
            }
        }

        // Component can be requested for install by several component (as a dependency).
        // Keep the reference count to a component, so we know when component needs to be
        // removed from install.
        if (!revertFromInstall && m_toInstallComponentIds.contains(dependencyComponentName)) {
            QStringList value = m_referenceCount.value(dependencyComponentName, QStringList());
            if (!value.contains(component->name()))
                value << component->name();
            m_referenceCount.insert(dependencyComponentName, value);
        }
        if (revertFromInstall) {
            if (m_toInstallComponentIds.contains(dependencyComponentName)
                    && m_referenceCount.contains(dependencyComponentName)) {
                QStringList value = m_referenceCount.value(dependencyComponentName);
                if (value.contains(component->name()))
                    value.removeOne(component->name());
                if (value.isEmpty())
                    m_referenceCount.remove(dependencyComponentName);
                else
                    m_referenceCount.insert(dependencyComponentName, value);
            }
            insertInstallReason(dependencyComponent, InstallerCalculator::Dependent, component->name(), true);
            if (!appendComponentToInstall(dependencyComponent, requiredDependencyVersion, revertFromInstall))
                return false;
            m_visitedComponents.remove(component);
        }

        //Check dependencies only if
        //- Dependency is not installed or update requested, nor newer version of dependency component required
        //- And dependency component is not already added for install
        //- And component is not already added for install, then dependencies are already resolved
        if (!revertFromInstall && ((!dependencyComponent->isInstalled() || dependencyComponent->updateRequested())
                || isUpdateRequired) && (!m_toInstallComponentIds.contains(dependencyComponent->name())
                && !m_toInstallComponentIds.contains(component->name()))) {
            if (m_visitedComponents.value(component).contains(dependencyComponent)) {
                const QString errorMessage = recursionError(component);
                qCWarning(QInstaller::lcInstallerInstallLog).noquote() << errorMessage;
                m_componentsToInstallError = errorMessage;
                Q_ASSERT_X(!m_visitedComponents.value(component).contains(dependencyComponent),
                    Q_FUNC_INFO, qPrintable(errorMessage));
                return false;
            }
            m_visitedComponents[component].insert(dependencyComponent);

            QStringList value = m_referenceCount.value(dependencyComponentName, QStringList());
            if (!value.contains(component->name()))
                value << component->name();
            m_referenceCount.insert(dependencyComponentName, value);
            insertInstallReason(dependencyComponent, InstallerCalculator::Dependent, component->name());
            if (!appendComponentToInstall(dependencyComponent, requiredDependencyVersion, revertFromInstall))
                return false;
        }
    }
    if (!revertFromInstall && !m_toInstallComponentIds.contains(component->name())) {
        realAppendToInstallComponents(component, requiredDependencyVersion, revertFromInstall);
        insertInstallReason(component, InstallerCalculator::Resolved);
    }
    if (revertFromInstall && m_toInstallComponentIds.contains(component->name())) {
        realAppendToInstallComponents(component, requiredDependencyVersion, revertFromInstall);
    }
    return true;
}

QString InstallerCalculator::recursionError(const Component *component) const
{
    return QCoreApplication::translate("InstallerCalculator", "Recursion detected, component \"%1\" "
        "already added with reason: \"%2\"").arg(component->name(), installReason(component));
}

QSet<Component *> InstallerCalculator::autodependencyComponents(const bool revertFromInstall)
{
    // All regular dependencies are resolved. Now we are looking for auto depend on components.
    // m_componentsForAutodepencencyCheck is a list of recently calculated components to be installed
    // (normal components and regular dependencies components), and we check possible installable auto
    // dependency components based on that list.
    QSet<Component *> foundAutoDependOnList;
    for (const Component *component : qAsConst(m_componentsForAutodepencencyCheck)) {
        if (!m_autoDependencyComponentHash.contains(component->name()))
            continue;
        for (const QString& autoDependency : m_autoDependencyComponentHash.value(component->name())) {
            // If a components is already installed or is scheduled for installation, no need to check
            // for auto depend installation.
            if ((!revertFromInstall && m_toInstallComponentIds.contains(autoDependency))
                || (revertFromInstall && !m_toInstallComponentIds.contains(autoDependency))) {
                continue;
            }
            Component *autoDependComponent = PackageManagerCore::componentByName(autoDependency, m_allComponents);
            if (!autoDependComponent)
                continue;
            if ((!autoDependComponent->isInstalled()
                 || (m_core->isUpdater() && autoDependComponent->isUpdateAvailable()))
                && !m_toInstallComponentIds.contains(autoDependComponent->name())) {
                // One of the components autodependons is requested for install, check if there
                // are other autodependencies as well
                if (autoDependComponent->isAutoDependOn(m_toInstallComponentIds)) {
                    foundAutoDependOnList.insert(autoDependComponent);
                    insertInstallReason(autoDependComponent, InstallerCalculator::Automatic);
                }
            } else if (revertFromInstall && m_toInstallComponentIds.contains(autoDependComponent->name())) {
                foundAutoDependOnList.insert(autoDependComponent);
            }
        }
    }
    m_componentsForAutodepencencyCheck.clear();
    return foundAutoDependOnList;
}

} // namespace QInstaller
