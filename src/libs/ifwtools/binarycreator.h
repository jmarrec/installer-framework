/* Copyright (C) 2026 The Qt Company Ltd.
 *
 * SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0
*/

#ifndef BINARYCREATOR_H
#define BINARYCREATOR_H

#include "ifwtools_global.h"

#include "repositorygen.h"
#include "fileutils.h"
#include "binaryformat.h"

#include <abstractarchive.h>

#include <QtCore/QString>
#include <QtCore/QFile>

namespace QInstallerTools {

struct Input
{
    QString outputPath;
    QString installerExePath;
    QInstallerTools::PackageInfoVector packages;
    QInstaller::ResourceCollectionManager manager;
};

typedef QInstaller::AbstractArchive::CompressionLevel Compression;

struct IFWTOOLS_EXPORT BinaryCreatorArgs
{
    QString target;
    QString configFile;
    QString templateBinary;
    QStringList packagesDirectories;
    QStringList repositoryDirectories;
    QString archiveSuffix = QLatin1String("7z");
    Compression compression = Compression::Normal;
    bool onlineOnly = false;
    bool offlineOnly = false;
    bool hybridInstaller = false;
    QStringList resources;
    QStringList filteredPackages;
    FilterType ftype = QInstallerTools::Exclude;
    bool compileResource = false;
    QString signingIdentity;
    QString signingOptions;
    QString signingScriptCmd;
    bool createMaintenanceTool = false;
};

class BundleBackup
{
public:
    explicit BundleBackup(const QString &bundle = QString())
        : bundle(bundle)
    {
        if (!bundle.isEmpty() && QFileInfo::exists(bundle)) {
            backup = QInstaller::generateTemporaryFileName(bundle);
            QFile::rename(bundle, backup);
        }
    }

    ~BundleBackup()
    {
        if (!backup.isEmpty()) {
            QInstaller::removeDirectory(bundle);
            QFile::rename(backup, bundle);
        }
    }

    void release() const
    {
        if (!backup.isEmpty())
            QInstaller::removeDirectory(backup);
        backup.clear();
    }

private:
    const QString bundle;
    mutable QString backup;
};

class WorkingDirectoryChange
{
public:
    explicit WorkingDirectoryChange(const QString &path)
        : oldPath(QDir::currentPath())
    {
        QDir::setCurrent(path);
    }

    virtual ~WorkingDirectoryChange()
    {
        QDir::setCurrent(oldPath);
    }

private:
    const QString oldPath;
};

void copyConfigData(const BinaryCreatorArgs &args, const QString &targetDir);
void copyHighDPIImage(const QFileInfo &childFileInfo, const QString &childName, const QString &targetFile);

int IFWTOOLS_EXPORT createBinary(BinaryCreatorArgs args, QString &argumentError);
void IFWTOOLS_EXPORT createMTDatFile(QFile &datFile);

} // namespace QInstallerTools

#endif // BINARYCREATOR_H
