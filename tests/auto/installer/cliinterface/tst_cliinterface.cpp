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
#include "../shared/packagemanager.h"
#include "../shared/verifyinstaller.h"

#include <component.h>
#include <packagemanagercore.h>

#include <QLoggingCategory>
#include <QTest>

#include <iostream>
#include <sstream>

using namespace QInstaller;

class tst_CLIInterface : public QObject
{
    Q_OBJECT

private slots:
    void testListAvailablePackages()
    {
        QString loggingRules = (QLatin1String("ifw.* = false\n"));

        QTest::ignoreMessage(QtDebugMsg, "Operations sanity check succeeded.");

        QScopedPointer<PackageManagerCore> core(PackageManager::getPackageManager
                (m_installDir, ":///data/repository"));

        QLoggingCategory::setFilterRules(loggingRules);
        auto func = &PackageManagerCore::listAvailablePackages;

        verifyListPackagesMessage(core.get(), QLatin1String("<?xml version=\"1.0\"?>\n"
            "<availablepackages>\n"
            "    <package name=\"AB\" displayname=\"AB\" version=\"1.0.2-1\"/>\n"
            "    <package name=\"A\" displayname=\"A\" version=\"1.0.2-1\"/>\n"
            "    <package name=\"B\" displayname=\"B\" version=\"1.0.0-1\"/>\n"
            "    <package name=\"C\" displayname=\"C\" version=\"1.0.0-1\"/>\n"
            "</availablepackages>\n"), func, QLatin1String("."), QHash<QString, QString>());

        verifyListPackagesMessage(core.get(), QLatin1String("<?xml version=\"1.0\"?>\n"
            "<availablepackages>\n"
            "    <package name=\"AB\" displayname=\"AB\" version=\"1.0.2-1\"/>\n"
            "    <package name=\"A\" displayname=\"A\" version=\"1.0.2-1\"/>\n"
            "</availablepackages>\n"), func, QLatin1String("A"), QHash<QString, QString>());

        verifyListPackagesMessage(core.get(), QLatin1String("<?xml version=\"1.0\"?>\n"
            "<availablepackages>\n"
            "    <package name=\"AB\" displayname=\"AB\" version=\"1.0.2-1\"/>\n"
            "    <package name=\"A\" displayname=\"A\" version=\"1.0.2-1\"/>\n"
            "</availablepackages>\n"), func, QLatin1String("A.*"), QHash<QString, QString>());

        verifyListPackagesMessage(core.get(), QLatin1String("<?xml version=\"1.0\"?>\n"
            "<availablepackages>\n"
            "    <package name=\"B\" displayname=\"B\" version=\"1.0.0-1\"/>\n"
            "</availablepackages>\n"), func, QLatin1String("^B"), QHash<QString, QString>());

        verifyListPackagesMessage(core.get(), QLatin1String("<?xml version=\"1.0\"?>\n"
            "<availablepackages>\n"
            "    <package name=\"B\" displayname=\"B\" version=\"1.0.0-1\"/>\n"
            "</availablepackages>\n"), func, QLatin1String("^B.*"), QHash<QString, QString>());

        verifyListPackagesMessage(core.get(), QLatin1String("<?xml version=\"1.0\"?>\n"
            "<availablepackages>\n"
            "    <package name=\"C\" displayname=\"C\" version=\"1.0.0-1\"/>\n"
            "</availablepackages>\n"), func, QLatin1String("^C"), QHash<QString, QString>());

        // Test with filters
        QHash<QString, QString> searchHash {
            { "Version", "1.0.2" },
            { "DisplayName", "A" }
        };
        verifyListPackagesMessage(core.get(), QLatin1String("<?xml version=\"1.0\"?>\n"
             "<availablepackages>\n"
             "    <package name=\"AB\" displayname=\"AB\" version=\"1.0.2-1\"/>\n"
             "    <package name=\"A\" displayname=\"A\" version=\"1.0.2-1\"/>\n"
             "</availablepackages>\n"), func, QString(), searchHash);

        searchHash.clear();
        searchHash.insert("Default", "false");
        verifyListPackagesMessage(core.get(), QLatin1String("<?xml version=\"1.0\"?>\n"
             "<availablepackages>\n"
             "    <package name=\"B\" displayname=\"B\" version=\"1.0.0-1\"/>\n"
             "</availablepackages>\n"), func, QString(), searchHash);

        // Need to change rules here to catch messages
        QLoggingCategory::setFilterRules("ifw.* = true\n");

        QTest::ignoreMessage(QtDebugMsg, "No matching packages found.");
        core->listAvailablePackages(QLatin1String("C.virt"));

        QTest::ignoreMessage(QtDebugMsg, "No matching packages found.");
        core->listAvailablePackages(QLatin1String("C.virt.subcomponent"));
    }

    void testInstallPackageFails()
    {
        QString loggingRules = (QLatin1String("ifw.* = false\n"
                                "ifw.installer.installlog = true\n"));

        QScopedPointer<PackageManagerCore> core(PackageManager::getPackageManager
                (m_installDir, ":///data/uninstallableComponentsRepository"));

        QLoggingCategory::setFilterRules(loggingRules);

        QTest::ignoreMessage(QtDebugMsg, "Preparing meta information download...");
        QTest::ignoreMessage(QtDebugMsg, "Cannot install component A. Component is installed only as automatic dependency to autoDep.\n");
        QCOMPARE(PackageManagerCore::Canceled, core->installSelectedComponentsSilently(QStringList()
                << QLatin1String("A")));

        QTest::ignoreMessage(QtDebugMsg, "Preparing meta information download...");
        QTest::ignoreMessage(QtDebugMsg, "Cannot install component AB. Component is not checkable, meaning you have to select one of the subcomponents.\n");
        QCOMPARE(PackageManagerCore::Canceled, core->installSelectedComponentsSilently(QStringList()
                << QLatin1String("AB")));

        QTest::ignoreMessage(QtDebugMsg, "Preparing meta information download...");
        QTest::ignoreMessage(QtDebugMsg, "Cannot install B. Component is virtual.\n");
        QCOMPARE(PackageManagerCore::Canceled, core->installSelectedComponentsSilently(QStringList()
                << QLatin1String("B")));

        QTest::ignoreMessage(QtDebugMsg, "Preparing meta information download...");
        QTest::ignoreMessage(QtDebugMsg, "Cannot install B.subcomponent. Component is a descendant of a virtual component B.\n");
        QCOMPARE(PackageManagerCore::Canceled, core->installSelectedComponentsSilently(QStringList()
                << QLatin1String("B.subcomponent")));

        QTest::ignoreMessage(QtDebugMsg, "Preparing meta information download...");
        QTest::ignoreMessage(QtDebugMsg, "Cannot install MissingComponent. Component not found.\n");
        QCOMPARE(PackageManagerCore::Canceled, core->installSelectedComponentsSilently(QStringList()
                << QLatin1String("MissingComponent")));
        QCOMPARE(PackageManagerCore::Canceled, core->status());
    }

    void testUninstallPackageFails()
    {
        QString loggingRules = (QLatin1String("ifw.installer.installog = true\n"));
        PackageManagerCore core;
        core.setPackageManager();
        QString appFilePath = QCoreApplication::applicationFilePath();
        core.setAllowedRunningProcesses(QStringList() << appFilePath);
        QLoggingCategory::setFilterRules(loggingRules);

        m_installDir = QInstaller::generateTemporaryFileName();
        QVERIFY(QDir().mkpath(m_installDir));
        QVERIFY(QFile::copy(":/data/componentsFromInstallPackagesRepository.xml", m_installDir + "/components.xml"));

        core.setValue(scTargetDir, m_installDir);
        QTest::ignoreMessage(QtWarningMsg, "Cannot uninstall ForcedInstallation component componentE");
        QCOMPARE(PackageManagerCore::Success, core.uninstallComponentsSilently(QStringList()
                << "componentE"));

        QTest::ignoreMessage(QtWarningMsg, "Cannot uninstall component componentD because it is added as auto dependency to componentA,componentB");
        QCOMPARE(PackageManagerCore::Success, core.uninstallComponentsSilently(QStringList()
                << "componentD"));

        QTest::ignoreMessage(QtWarningMsg, "Cannot uninstall component MissingComponent. Component not found in install tree.");
        QCOMPARE(PackageManagerCore::Success, core.uninstallComponentsSilently(QStringList()
                << "MissingComponent"));

        QTest::ignoreMessage(QtWarningMsg, "Cannot uninstall virtual component componentH");
        QCOMPARE(PackageManagerCore::Success, core.uninstallComponentsSilently(QStringList()
                << "componentH"));

        QCOMPARE(PackageManagerCore::Success, core.status());
    }

    void testListInstalledPackages()
    {
        QString loggingRules = (QLatin1String("ifw.* = false\n"));
        PackageManagerCore core;
        core.setPackageManager();
        QLoggingCategory::setFilterRules(loggingRules);
        auto func = &PackageManagerCore::listInstalledPackages;

        const QString testDirectory = QInstaller::generateTemporaryFileName();
        QVERIFY(QDir().mkpath(testDirectory));
        QVERIFY(QFile::copy(":/data/components.xml", testDirectory + "/components.xml"));

        core.setValue(scTargetDir, testDirectory);

        verifyListPackagesMessage(&core, QLatin1String("<?xml version=\"1.0\"?>\n"
            "<localpackages>\n"
            "    <package name=\"A\" displayname=\"A Title\" version=\"1.0.2-1\"/>\n"
            "    <package name=\"B\" displayname=\"B Title\" version=\"1.0.0-1\"/>\n"
            "</localpackages>\n"), func, QString());

        verifyListPackagesMessage(&core, QLatin1String("<?xml version=\"1.0\"?>\n"
            "<localpackages>\n"
            "    <package name=\"A\" displayname=\"A Title\" version=\"1.0.2-1\"/>\n"
            "</localpackages>\n"), func, QLatin1String("A"));

        QDir dir(testDirectory);
        QVERIFY(dir.removeRecursively());
    }

    void testNoDefaultInstallations()
    {
        QScopedPointer<PackageManagerCore> core(PackageManager::getPackageManagerWithInit
                (m_installDir, ":///data/installPackagesRepository"));
        core->setNoDefaultInstallation(true);
        QCOMPARE(PackageManagerCore::Success, core->installDefaultComponentsSilently());
        QCOMPARE(PackageManagerCore::Success, core->status());
        VerifyInstaller::verifyFileExistence(m_installDir, QStringList() << "components.xml"
                            << "installcontentE.txt");
        core->setNoDefaultInstallation(false);
    }

    void testInstallForcedPackageSilently()
    {
        QScopedPointer<PackageManagerCore> core(PackageManager::getPackageManagerWithInit
                (m_installDir, ":///data/installPackagesRepository"));
        QCOMPARE(PackageManagerCore::Success, core->installSelectedComponentsSilently(QStringList()
                << QLatin1String("componentE")));
        QCOMPARE(PackageManagerCore::Success, core->status());
        VerifyInstaller::verifyInstallerResources(m_installDir, "componentA", "1.0.0content.txt");
        VerifyInstaller::verifyInstallerResources(m_installDir, "componentE", "1.0.0content.txt"); //ForcedInstall
        VerifyInstaller::verifyInstallerResources(m_installDir, "componentG", "1.0.0content.txt"); //Depends on componentA
        VerifyInstaller::verifyFileExistence(m_installDir, QStringList() << "components.xml" << "installcontent.txt"
                            << "installcontentA.txt" << "installcontentE.txt" << "installcontentG.txt");
    }

    void testInstallPackageSilently()
    {
        QScopedPointer<PackageManagerCore> core(PackageManager::getPackageManagerWithInit
                (m_installDir, ":///data/installPackagesRepository"));
        QCOMPARE(PackageManagerCore::Success, core->installSelectedComponentsSilently(QStringList()
                << QLatin1String("componentA")));
        QCOMPARE(PackageManagerCore::Success, core->status());
        VerifyInstaller::verifyInstallerResources(m_installDir, "componentA", "1.0.0content.txt");
        VerifyInstaller::verifyInstallerResources(m_installDir, "componentE", "1.0.0content.txt"); //ForcedInstall
        VerifyInstaller::verifyInstallerResources(m_installDir, "componentG", "1.0.0content.txt"); //Depends on componentA
        VerifyInstaller::verifyFileExistence(m_installDir, QStringList() << "components.xml" << "installcontent.txt"
                            << "installcontentA.txt" << "installcontentE.txt" << "installcontentG.txt");
    }

    void testUninstallPackageSilently()
    {
        QScopedPointer<PackageManagerCore> core(PackageManager::getPackageManagerWithInit
                (m_installDir, ":///data/installPackagesRepository"));
        QCOMPARE(PackageManagerCore::Success, core->installSelectedComponentsSilently(QStringList()
                << QLatin1String("componentA")));
        VerifyInstaller::verifyFileExistence(m_installDir, QStringList() << "components.xml" << "installcontentE.txt"
                << "installcontentA.txt" << "installcontent.txt" << "installcontentG.txt");

        core->commitSessionOperations();
        core->setPackageManager();
        QCOMPARE(PackageManagerCore::Success, core->uninstallComponentsSilently(QStringList()
                << QLatin1String("componentA")));
        QCOMPARE(PackageManagerCore::Success, core->status());
        VerifyInstaller::verifyInstallerResources(m_installDir, "componentE", "1.0.0content.txt"); //ForcedInstall
        VerifyInstaller::verifyInstallerResourcesDeletion(m_installDir, "componentA");
        VerifyInstaller::verifyInstallerResourcesDeletion(m_installDir, "componentG"); //Depends on componentA
        VerifyInstaller::verifyFileExistence(m_installDir, QStringList() << "components.xml" << "installcontentE.txt");
    }

    void testRemoveAllSilently()
    {
        QScopedPointer<PackageManagerCore> core(PackageManager::getPackageManagerWithInit
                (m_installDir, ":///data/installPackagesRepository"));
        QCOMPARE(PackageManagerCore::Success, core->installSelectedComponentsSilently(QStringList()
            << QLatin1String("componentA")));
        VerifyInstaller::verifyFileExistence(m_installDir, QStringList() << "components.xml" << "installcontentE.txt"
                            << "installcontentA.txt" << "installcontent.txt" << "installcontentG.txt");

        core->commitSessionOperations();
        core->setUninstaller();
        QCOMPARE(PackageManagerCore::Success, core->removeInstallationSilently());
        QCOMPARE(PackageManagerCore::Success, core->status());
        VerifyInstaller::verifyInstallerResourcesDeletion(m_installDir, "componentA");
        VerifyInstaller::verifyInstallerResourcesDeletion(m_installDir, "componentE");
        VerifyInstaller::verifyInstallerResourcesDeletion(m_installDir, "componentG");

        // On Windows we have to settle for the resources check above as maintenance
        // tool (if it would exists) and target directory are only removed later via
        // started VBScript process. On Unix platforms the target directory should
        // be removed in PackageManagerCorePrivate::runUninstaller().
#if defined(Q_OS_UNIX)
        QVERIFY(!QDir(m_installDir).exists());
#endif
    }

    void testInstallWithDependencySilently()
    {
        QScopedPointer<PackageManagerCore> core(PackageManager::getPackageManagerWithInit
                (m_installDir, ":///data/installPackagesRepository"));
        QCOMPARE(PackageManagerCore::Success, core->installSelectedComponentsSilently(QStringList()
                << QLatin1String("componentC")));
        QCOMPARE(PackageManagerCore::Success, core->status());
        VerifyInstaller::verifyInstallerResources(m_installDir, "componentA", "1.0.0content.txt"); //Dependency for componentC
        VerifyInstaller::verifyInstallerResources(m_installDir, "componentB", "1.0.0content.txt"); //Dependency for componentC
        VerifyInstaller::verifyInstallerResources(m_installDir, "componentE", "1.0.0content.txt"); //ForcedInstall
        VerifyInstaller::verifyInstallerResources(m_installDir, "componentG", "1.0.0content.txt"); //Depends on componentA
        VerifyInstaller::verifyInstallerResources(m_installDir, "componentI", "1.0.0content.txt"); //Virtual, depends on componentC
        VerifyInstaller::verifyInstallerResources(m_installDir, "componentD", "1.0.0content.txt"); //Autodepend on componentA and componentB
        VerifyInstaller::verifyFileExistence(m_installDir, QStringList() << "components.xml" << "installcontentC.txt"
                            << "installcontent.txt" << "installcontentA.txt" << "installcontentB.txt"
                            << "installcontentD.txt"<< "installcontentE.txt" << "installcontentG.txt" << "installcontentI.txt");
    }

    void testUninstallWithDependencySilently()
    {
        QScopedPointer<PackageManagerCore> core(PackageManager::getPackageManagerWithInit
                (m_installDir, ":///data/installPackagesRepository"));
        QCOMPARE(PackageManagerCore::Success, core->installSelectedComponentsSilently(QStringList()
                << QLatin1String("componentC")));
        VerifyInstaller::verifyFileExistence(m_installDir, QStringList() << "components.xml" << "installcontentC.txt"
                            << "installcontent.txt" << "installcontentA.txt" << "installcontentB.txt"
                            << "installcontentD.txt"<< "installcontentE.txt" << "installcontentG.txt" << "installcontentI.txt");

        core->commitSessionOperations();
        core->setPackageManager();
        QCOMPARE(PackageManagerCore::Success, core->uninstallComponentsSilently(QStringList()
                << QLatin1String("componentC")));
        QCOMPARE(PackageManagerCore::Success, core->status());
        VerifyInstaller::verifyInstallerResources(m_installDir, "componentA", "1.0.0content.txt"); //Dependency for componentC
        VerifyInstaller::verifyInstallerResources(m_installDir, "componentB", "1.0.0content.txt"); //Dependency for componentC
        VerifyInstaller::verifyInstallerResources(m_installDir, "componentE", "1.0.0content.txt"); //ForcedInstall
        VerifyInstaller::verifyInstallerResources(m_installDir, "componentG", "1.0.0content.txt"); //Depends on componentA
        VerifyInstaller::verifyInstallerResources(m_installDir, "componentD", "1.0.0content.txt"); //Autodepend on componentA and componentB
        VerifyInstaller::verifyInstallerResourcesDeletion(m_installDir, "componentC");
        VerifyInstaller::verifyInstallerResourcesDeletion(m_installDir, "componentI"); //Virtual, depends on componentC
        VerifyInstaller::verifyFileExistence(m_installDir, QStringList() << "components.xml"
                            << "installcontent.txt" << "installcontentA.txt" << "installcontentB.txt"
                            << "installcontentD.txt"<< "installcontentE.txt" << "installcontentG.txt");
    }

    void testInstallSubcomponentSilently()
    {
        QScopedPointer<PackageManagerCore> core(PackageManager::getPackageManagerWithInit
                (m_installDir, ":///data/installPackagesRepository"));
        QCOMPARE(PackageManagerCore::Success, core->installSelectedComponentsSilently(QStringList()
                << QLatin1String("componentF.subcomponent2.subsubcomponent2")));
        QCOMPARE(PackageManagerCore::Success, core->status());
        VerifyInstaller::verifyInstallerResources(m_installDir, "componentF.subcomponent2.subsubcomponent2", "1.0.0content.txt");
        VerifyInstaller::verifyInstallerResources(m_installDir, "componentF.subcomponent2", "1.0.0content.txt");
        VerifyInstaller::verifyInstallerResources(m_installDir, "componentF", "1.0.0content.txt");
        VerifyInstaller::verifyInstallerResources(m_installDir, "componentA", "1.0.0content.txt"); //Dependency for componentG
        VerifyInstaller::verifyInstallerResources(m_installDir, "componentE", "1.0.0content.txt"); //ForcedInstall
        VerifyInstaller::verifyInstallerResources(m_installDir, "componentG", "1.0.0content.txt"); //Default install
        VerifyInstaller::verifyFileExistence(m_installDir, QStringList() << "components.xml" << "installcontentF.txt"
                            << "installcontentF_2.txt"  << "installcontentF_2_2.txt"
                            << "installcontent.txt" << "installcontentA.txt"
                            << "installcontentE.txt" << "installcontentG.txt");
    }

    void testUninstallSubcomponentSilently()
    {
        QScopedPointer<PackageManagerCore> core(PackageManager::getPackageManagerWithInit
                (m_installDir, ":///data/installPackagesRepository"));
        QCOMPARE(PackageManagerCore::Success, core->installSelectedComponentsSilently(QStringList()
                << QLatin1String("componentF.subcomponent2.subsubcomponent2")));
        QCOMPARE(PackageManagerCore::Success, core->status());
        VerifyInstaller::verifyFileExistence(m_installDir, QStringList() << "components.xml" << "installcontentF.txt"
                            << "installcontentF_2.txt"  << "installcontentF_2_2.txt"
                            << "installcontent.txt" << "installcontentA.txt"
                            << "installcontentE.txt" << "installcontentG.txt");
        core->commitSessionOperations();
        core->setPackageManager();
        QCOMPARE(PackageManagerCore::Success, core->uninstallComponentsSilently(QStringList()
                << QLatin1String("componentF.subcomponent2")));
        QCOMPARE(PackageManagerCore::Success, core->status());
        VerifyInstaller::verifyInstallerResources(m_installDir, "componentA", "1.0.0content.txt"); //Dependency for componentG
        VerifyInstaller::verifyInstallerResources(m_installDir, "componentE", "1.0.0content.txt"); //ForcedInstall
        VerifyInstaller::verifyInstallerResources(m_installDir, "componentG", "1.0.0content.txt"); //Default install
        VerifyInstaller::verifyInstallerResourcesDeletion(m_installDir, "componentF.subcomponent2.subsubcomponent2");
        VerifyInstaller::verifyInstallerResourcesDeletion(m_installDir, "componentF.subcomponent2");
        VerifyInstaller::verifyInstallerResourcesDeletion(m_installDir, "componentF");
        VerifyInstaller::verifyFileExistence(m_installDir, QStringList() << "components.xml"
                            << "installcontent.txt" << "installcontentA.txt"
                            << "installcontentE.txt" << "installcontentG.txt");
    }

    void testInstallDefaultPackagesSilently()
    {
        QScopedPointer<PackageManagerCore> core(PackageManager::getPackageManagerWithInit
                (m_installDir, ":///data/installPackagesRepository"));
        QCOMPARE(PackageManagerCore::Success, core->installDefaultComponentsSilently());
        QCOMPARE(PackageManagerCore::Success, core->status());
        VerifyInstaller::verifyInstallerResources(m_installDir, "componentA", "1.0.0content.txt"); //Dependency for componentG
        VerifyInstaller::verifyInstallerResources(m_installDir, "componentE", "1.0.0content.txt"); //ForcedInstall
        VerifyInstaller::verifyInstallerResources(m_installDir, "componentG", "1.0.0content.txt"); //Default
        VerifyInstaller::verifyFileExistence(m_installDir, QStringList() << "components.xml" << "installcontent.txt"
                            << "installcontentA.txt" << "installcontentE.txt" << "installcontentG.txt");
    }

    void testUnInstallDefaultPackagesSilently()
    {
        QScopedPointer<PackageManagerCore> core(PackageManager::getPackageManagerWithInit
                (m_installDir, ":///data/installPackagesRepository"));
        QCOMPARE(PackageManagerCore::Success, core->installDefaultComponentsSilently());
        QCOMPARE(PackageManagerCore::Success, core->status());
        VerifyInstaller::verifyFileExistence(m_installDir, QStringList() << "components.xml" << "installcontent.txt"
                            << "installcontentA.txt" << "installcontentE.txt" << "installcontentG.txt");

        core->commitSessionOperations();
        core->setPackageManager();
        QCOMPARE(PackageManagerCore::Success, core->uninstallComponentsSilently(QStringList()
                << "componentG"));
        QCOMPARE(PackageManagerCore::Success, core->status());
        VerifyInstaller::verifyInstallerResources(m_installDir, "componentA", "1.0.0content.txt"); //Dependency for componentG
        VerifyInstaller::verifyInstallerResources(m_installDir, "componentE", "1.0.0content.txt"); //ForcedInstall
        VerifyInstaller::verifyInstallerResourcesDeletion(m_installDir, "componentG");
        VerifyInstaller::verifyFileExistence(m_installDir, QStringList() << "components.xml" << "installcontent.txt"
                            << "installcontentA.txt" << "installcontentE.txt");
    }

    void testUninstallForcedPackagesSilenly()
    {
        QScopedPointer<PackageManagerCore> core(PackageManager::getPackageManagerWithInit
                (m_installDir, ":///data/installPackagesRepository"));
        QCOMPARE(PackageManagerCore::Success, core->installDefaultComponentsSilently());
        QCOMPARE(PackageManagerCore::Success, core->status());
        core->commitSessionOperations();
        core->setPackageManager();
        QCOMPARE(PackageManagerCore::Success, core->uninstallComponentsSilently(QStringList()
                << "componentE"));
        QCOMPARE(PackageManagerCore::Success, core->status());
        //Nothing is uninstalled as componentE is forced install and cannot be uninstalled
        VerifyInstaller::verifyInstallerResources(m_installDir, "componentA", "1.0.0content.txt"); //Dependency for componentG
        VerifyInstaller::verifyInstallerResources(m_installDir, "componentE", "1.0.0content.txt"); //ForcedInstall
        VerifyInstaller::verifyInstallerResources(m_installDir, "componentG", "1.0.0content.txt"); //Default
        VerifyInstaller::verifyFileExistence(m_installDir, QStringList() << "components.xml" << "installcontent.txt"
                            << "installcontentA.txt" << "installcontentE.txt" << "installcontentG.txt");
    }

    void testUninstallAutodependencyPackagesSilenly()
    {
        QScopedPointer<PackageManagerCore> core(PackageManager::getPackageManagerWithInit
                (m_installDir, ":///data/installPackagesRepository"));
        QCOMPARE(PackageManagerCore::Success, core->installSelectedComponentsSilently(QStringList()
                << "componentA" << "componentB"));
        QCOMPARE(PackageManagerCore::Success, core->status());
        core->commitSessionOperations();
        core->setPackageManager();
        QCOMPARE(PackageManagerCore::Success, core->uninstallComponentsSilently(QStringList()
                << "componentD"));
        QCOMPARE(PackageManagerCore::Success, core->status());
        //Nothing is uninstalled as componentD is installed as autodependency to componentA and componentB
        VerifyInstaller::verifyInstallerResources(m_installDir, "componentA", "1.0.0content.txt");
        VerifyInstaller::verifyInstallerResources(m_installDir, "componentB", "1.0.0content.txt");
        VerifyInstaller::verifyInstallerResources(m_installDir, "componentD", "1.0.0content.txt");
        VerifyInstaller::verifyInstallerResources(m_installDir, "componentE", "1.0.0content.txt"); //ForcedInstall
        VerifyInstaller::verifyInstallerResources(m_installDir, "componentG", "1.0.0content.txt"); //Default
        VerifyInstaller::verifyFileExistence(m_installDir, QStringList() << "components.xml" << "installcontent.txt"
                            << "installcontentA.txt" << "installcontentE.txt" << "installcontentG.txt"
                            << "installcontentB.txt" << "installcontentD.txt");
    }

    void testUninstallVirtualSetVisibleSilently()
    {
        QScopedPointer<PackageManagerCore> core(PackageManager::getPackageManagerWithInit
                (m_installDir, ":///data/installPackagesRepository"));
        core->setVirtualComponentsVisible(true);
        QCOMPARE(PackageManagerCore::Success, core->installSelectedComponentsSilently(QStringList()
                <<"componentH"));
        QCOMPARE(PackageManagerCore::Success, core->status());
        VerifyInstaller::verifyInstallerResources(m_installDir, "componentH", "1.0.0content.txt");

        core->commitSessionOperations();
        core->setPackageManager();
        QCOMPARE(PackageManagerCore::Success, core->uninstallComponentsSilently(QStringList()
                << "componentH"));
        QCOMPARE(PackageManagerCore::Success, core->status());
        VerifyInstaller::verifyInstallerResourcesDeletion(m_installDir, "componentH");
    }

    void testFileQuery()
    {
        QScopedPointer<PackageManagerCore> core(PackageManager::getPackageManagerWithInit(m_installDir,
                                    ":///data/filequeryrepository"));
        core->setCommandLineInstance(true);
        core->setFileDialogAutomaticAnswer("ValidDirectory", m_installDir);

        QString testFile = qApp->applicationDirPath() + QDir::toNativeSeparators("/test");
        QFile file(testFile);
        QVERIFY(file.open(QIODevice::WriteOnly));
        core->setFileDialogAutomaticAnswer("ValidFile", testFile);

        //File dialog launched without ID
        core->setFileDialogAutomaticAnswer("GetExistingDirectory", m_installDir);
        core->setFileDialogAutomaticAnswer("GetExistingFile", testFile);

        QCOMPARE(PackageManagerCore::Success, core->installDefaultComponentsSilently());
        QCOMPARE(PackageManagerCore::Success, core->status());

        QVERIFY(core->containsFileDialogAutomaticAnswer("ValidFile"));
        core->removeFileDialogAutomaticAnswer("ValidFile");
        QVERIFY(!core->containsFileDialogAutomaticAnswer("ValidFile"));

        QVERIFY(file.remove());
        core->deleteLater();
    }

    void init()
    {
        m_installDir = QInstaller::generateTemporaryFileName();
        QVERIFY(QDir().mkpath(m_installDir));
    }

    void initTestCase()
    {
        qSetGlobalQHashSeed(0); //Ensures the dom document deterministic behavior
    }

    void cleanup()
    {
        QDir dir(m_installDir);
        QVERIFY(dir.removeRecursively());
    }

private:
    template <typename Func, typename... Args>
    void verifyListPackagesMessage(PackageManagerCore *core, const QString &message,
                                   Func func, Args... args)
    {
        std::ostringstream stream;
        std::streambuf *buf = std::cout.rdbuf();
        std::cout.rdbuf(stream.rdbuf());

        (core->*func)(std::forward<Args>(args)...);

        std::cout.rdbuf(buf);
        QVERIFY(stream && stream.str() == message.toStdString());
    }

private:
    QString m_installDir;
};


QTEST_MAIN(tst_CLIInterface)

#include "tst_cliinterface.moc"
