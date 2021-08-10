/**************************************************************************
**
** Copyright (C) 2020 The Qt Company Ltd.
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
#include "../../installer/shared/verifyinstaller.h"

#include <repositorygen.h>
#include <repositorygen.cpp>
#include <init.h>

#include <QFile>
#include <QTest>
#include <QRegularExpression>

class tst_repotest : public QObject
{
    Q_OBJECT
private:
    // TODO generateRepo() is almost direct copy from repogen.cpp.
    // Move the needed parts of repogen.cpp to a usable function for easier maintenance.
    void generateRepo(bool createSplitMetadata, bool createUnifiedMetadata, bool updateNewComponents)
    {
        QStringList filteredPackages;
        QInstallerTools::FilterType filterType = QInstallerTools::Exclude;

        QInstallerTools::PackageInfoVector precompressedPackages = QInstallerTools::createListOfRepositoryPackages
                (m_repositoryDirectories, &filteredPackages, filterType);
        m_packages.append(precompressedPackages);

        QStringList packageDirectories;
        if (!m_packagesDirectory.isEmpty())
            packageDirectories << m_packagesDirectory;

        QInstallerTools::PackageInfoVector preparedPackages = QInstallerTools::createListOfPackages(
            packageDirectories, &filteredPackages, filterType);
        m_packages.append(preparedPackages);

        if (updateNewComponents)
            QInstallerTools::filterNewComponents(m_repositoryDir, m_packages);
        QHash<QString, QString> pathToVersionMapping = QInstallerTools::buildPathToVersionMapping(m_packages);

        foreach (const QInstallerTools::PackageInfo &package, m_packages) {
            const QFileInfo fi(m_repositoryDir, package.name);
            if (fi.exists())
                removeDirectory(fi.absoluteFilePath());
        }

        if (updateNewComponents) { //Verify that component B exists as that is not updated
            if (createSplitMetadata) {
                VerifyInstaller::verifyFileExistence(m_repositoryDir + "/B", QStringList() << "1.0.0content.7z"
                                                    << "1.0.0content.7z.sha1" << "1.0.0meta.7z");
            } else {
                VerifyInstaller::verifyFileExistence(m_repositoryDir + "/B", QStringList() << "1.0.0content.7z"
                                                    << "1.0.0content.7z.sha1");
            }
        }
        QStringList directories;
        directories.append(packageDirectories);
        directories.append(m_repositoryDirectories);

        QStringList unite7zFiles;
        foreach (const QString &repositoryDirectory, m_repositoryDirectories) {
            QDirIterator it(repositoryDirectory, QStringList(QLatin1String("*_meta.7z"))
                            , QDir::Files | QDir::CaseSensitive);
            while (it.hasNext()) {
                it.next();
                unite7zFiles.append(it.fileInfo().absoluteFilePath());
            }
        }

        QInstallerTools::copyComponentData(directories, m_repositoryDir, &m_packages);
        QInstallerTools::copyMetaData(m_tmpMetaDir, m_repositoryDir, m_packages, QLatin1String("{AnyApplication}"),
            QLatin1String("1.0.0"), unite7zFiles);
        QString existing7z = QInstallerTools::existingUniteMeta7z(m_repositoryDir);
        if (!existing7z.isEmpty())
             existing7z = m_repositoryDir + QDir::separator() + existing7z;
        QInstallerTools::compressMetaDirectories(m_tmpMetaDir, existing7z, pathToVersionMapping,
                                                 createSplitMetadata, createUnifiedMetadata);
        QDirIterator it(m_repositoryDir, QStringList(QLatin1String("Updates*.xml"))
                        << QLatin1String("*_meta.7z"), QDir::Files | QDir::CaseSensitive);
        while (it.hasNext()) {
            it.next();
            QFile::remove(it.fileInfo().absoluteFilePath());
        }
        QInstaller::moveDirectoryContents(m_tmpMetaDir, m_repositoryDir);
    }

    void generateTempMetaDir()
    {
        if (!m_tmpMetaDir.isEmpty())
            m_tempDirDeleter.releaseAndDelete(m_tmpMetaDir);
        QTemporaryDir tmp;
        tmp.setAutoRemove(false);
        m_tmpMetaDir = tmp.path();
        m_tempDirDeleter.add(m_tmpMetaDir);
    }

    void clearData()
    {
        generateTempMetaDir();
        m_packagesDirectory.clear();
        m_repositoryDirectories.clear();
        m_packages.clear();
    }

    void initRepoUpdate()
    {
        clearData();
        m_packagesDirectory = ":///packages_update";
    }

    void initRepoUpdateFromRepository(const QString &repository)
    {
        clearData();
        m_repositoryDirectories << repository;
    }

    void verifyUniteMetadata(const QString &scriptVersion)
    {
        QString fileContent = VerifyInstaller::fileContent(m_repositoryDir + QDir::separator()
                                            + "Updates.xml");
        QRegularExpression re("<MetadataName>(.*)<.MetadataName>");
        QStringList matches = re.match(fileContent).capturedTexts();
        QString existingUniteMeta7z = QInstallerTools::existingUniteMeta7z(m_repositoryDir);
        QCOMPARE(2, matches.count());
        QCOMPARE(existingUniteMeta7z, matches.at(1));
        QFile file(m_repositoryDir + QDir::separator() + matches.at(1));
        QVERIFY(file.open(QIODevice::ReadOnly));

        //We have script<version>.qs for package A in the unite metadata
        QVector<Lib7z::File>::const_iterator fileIt;
        const QVector<Lib7z::File> files = Lib7z::listArchive(&file);
        for (fileIt = files.begin(); fileIt != files.end(); ++fileIt) {
            if (fileIt->isDirectory)
                continue;
            QString fileName = "A/script%1.qs";
            QCOMPARE(qPrintable(fileName.arg(scriptVersion)), fileIt->path);
        }

        VerifyInstaller::verifyFileExistence(m_repositoryDir, QStringList() << "Updates.xml"
                                            << matches.at(1));
        VerifyInstaller::verifyFileContent(m_repositoryDir + QDir::separator() + "Updates.xml",
                                            "SHA1");
        VerifyInstaller::verifyFileContent(m_repositoryDir + QDir::separator() + "Updates.xml",
                                            "MetadataName");
    }

    void verifyComponentRepository(const QString &componentAVersion, bool hasComponentMeta)
    {
        const QString content = "%1content.7z";
        const QString contentSha = "%1content.7z.sha1";
        const QString meta = "%1meta.7z";
        QStringList componentA;
        QStringList componentB;
        componentA << qPrintable(content.arg(componentAVersion)) << qPrintable(contentSha.arg(componentAVersion));
        componentB << "1.0.0content.7z" << "1.0.0content.7z.sha1";
        if (hasComponentMeta) {
            componentA << qPrintable(meta.arg(componentAVersion));
            componentB << "1.0.0meta.7z";
        }
        VerifyInstaller::verifyFileExistence(m_repositoryDir + "/A", componentA);
        VerifyInstaller::verifyFileExistence(m_repositoryDir + "/B", componentB);
    }

    void verifyComponentMetaUpdatesXml()
    {
        VerifyInstaller::verifyFileExistence(m_repositoryDir, QStringList() << "Updates.xml");
        VerifyInstaller::verifyFileHasNoContent(m_repositoryDir + QDir::separator() + "Updates.xml",
                                           "MetadataName");
    }

    void ignoreMessagesForComponentHash(const QStringList &components, bool update)
    {
        QString packageDir = m_packagesDirectory;
        packageDir.remove("//"); // e.g. :///packages -> :/packages
        foreach (const QString component, components) {
            QString message = "Copying component data for \"%1\"";
            QTest::ignoreMessage(QtDebugMsg, qPrintable(message.arg(component)));
            if (update)
                message = "Compressing files found in data directory: (\"%1/%2/data/%2_update.txt\")";
            else
                message = "Compressing files found in data directory: (\"%1/%2/data/%2.txt\")";
            QTest::ignoreMessage(QtDebugMsg, qPrintable(message.arg(packageDir, component)));
            QTest::ignoreMessage(QtDebugMsg, QRegularExpression("Hash is stored in *"));
            QTest::ignoreMessage(QtDebugMsg, QRegularExpression("Creating hash of archive *"));
            QTest::ignoreMessage(QtDebugMsg, QRegularExpression("Generated sha1 hash: *"));
        }
    }

    void ignoreMessagesForCopyRepository(const QString &component, const QString &version, const QString &repository)
    {
        QStringList contentFiles;
        contentFiles << "content.7z" << "content.7z.sha1";
        QString message = "Copying component data for \"%1\"";
        QTest::ignoreMessage(QtDebugMsg, qPrintable(message.arg(component)));
        foreach (const QString &fileName, contentFiles) {
            message = "Copying file from \":///%5/%1/%2%4\" to \"%3/%1/%2%4\"";
            QTest::ignoreMessage(QtDebugMsg, qPrintable(message.arg(component).arg(version)
                .arg(m_repositoryDir).arg(fileName).arg(repository)));
        }
    }

    void ignoreMessageForCollectingRepository(const QString &repository)
    {
        const QString message = "Process repository \"%2\"";
        QTest::ignoreMessage(QtDebugMsg, "Collecting information about available repository packages...");
        QTest::ignoreMessage(QtDebugMsg, qPrintable(message.arg(repository)));
        QTest::ignoreMessage(QtDebugMsg, "Collecting information about available packages...");
        QTest::ignoreMessage(QtDebugMsg, "No available packages found at the specified location.");
        QTest::ignoreMessage(QtDebugMsg, "- it provides the package \"A\"  -  \"2.0.0\"");
    }

    void ignoreMessagesForCopyMetadata(const QString &component, bool hasMeta, bool update)
    {
        QString message = "Copy meta data for package \"%2\" using \"%1/%2/meta/package.xml\"";
        QTest::ignoreMessage(QtDebugMsg, qPrintable(message.arg(m_packagesDirectory, component)));
        QTest::ignoreMessage(QtDebugMsg, QRegularExpression("calculate size of directory *"));
        if (hasMeta) {
            if (update)
                message = "Copying associated \"script\" file \"%1/%2/meta/script2.0.0.qs\"";
            else
                message = "Copying associated \"script\" file \"%1/%2/meta/script1.0.0.qs\"";
            QTest::ignoreMessage(QtDebugMsg, qPrintable(message.arg(m_packagesDirectory, component)));
            QTest::ignoreMessage(QtDebugMsg, "done.");
        }
    }

    void ignoreMessagesForComponentSha(const QStringList &components, bool clearOldChecksum)
    {
        foreach (const QString component, components) {
            const QString message = "Searching sha1sum node for \"%1\"";
            QTest::ignoreMessage(QtDebugMsg, qPrintable(message.arg(component)));
            if (clearOldChecksum)
                QTest::ignoreMessage(QtDebugMsg, QRegularExpression("- clearing the old sha1sum *"));
            QTest::ignoreMessage(QtDebugMsg, QRegularExpression("- writing the sha1sum *"));
        }
    }

    void ignoreMessagesForUniteMeta(bool update)
    {
        QTest::ignoreMessage(QtDebugMsg, "Writing sha1sum node.");
        if (update)
            QTest::ignoreMessage(QtDebugMsg, QRegularExpression("- clearing the old sha1sum *"));
        QTest::ignoreMessage(QtDebugMsg, QRegularExpression("- writing the sha1sum"));
        QTest::ignoreMessage(QtDebugMsg, QRegularExpression("Updating the metadata node with name *"));
    }

    void ignoreMessageForCollectingPackages(const QString &versionA = QString(),
            const QString &versionB = QString(), const QString &versionC = QString())
    {
        QTest::ignoreMessage(QtDebugMsg, "Collecting information about available repository packages...");
        QTest::ignoreMessage(QtDebugMsg, "Collecting information about available packages...");
        if (!versionA.isEmpty()) {
            QTest::ignoreMessage(QtDebugMsg, "Found subdirectory \"A\"");
            const QString message = "- it provides the package \"A\"  -  \"%1\"";
            QTest::ignoreMessage(QtDebugMsg, qPrintable(message.arg(versionA)));
        }
        if (!versionB.isEmpty()) {
            QTest::ignoreMessage(QtDebugMsg, "Found subdirectory \"B\"");
            const QString message = "- it provides the package \"B\"  -  \"%1\"";
            QTest::ignoreMessage(QtDebugMsg, qPrintable(message.arg(versionB)));
        }
        if (!versionC.isEmpty()) {
            QTest::ignoreMessage(QtDebugMsg, "Found subdirectory \"C\"");
            const QString message = "- it provides the package \"C\"  -  \"%1\"";
            QTest::ignoreMessage(QtDebugMsg, qPrintable(message.arg(versionC)));
        }
    }

    void ignoreMessageForCollectingPackagesFromRepository(const QString &versionA, const QString &versionB)
    {
        QString message = "- it provides the package \"B\"  -  \"%1\"";
        QTest::ignoreMessage(QtDebugMsg, qPrintable(message.arg(versionB)));
        message = "- it provides the package \"A\"  -  \"%1\"";
        QTest::ignoreMessage(QtDebugMsg, qPrintable(message.arg(versionA)));
        QTest::ignoreMessage(QtDebugMsg, "Collecting information about available packages...");
        QTest::ignoreMessage(QtDebugMsg, "No available packages found at the specified location.");
    }

    void ignoreMessagesForUpdateComponents()
    {
        ignoreMessageForCollectingPackages("2.0.0", "1.0.0");
        ignoreMessagesForComponentSha(QStringList() << "A" << "B", false);
        ignoreMessagesForComponentHash(QStringList() << "A" << "B", true);
        ignoreMessagesForCopyMetadata("A", true, true);
        ignoreMessagesForCopyMetadata("B", false, true);
    }

    void ignoreMessageForUpdateComponent()
    {
        QString message = "Update component \"A\" in \"%1\" .";
        QTest::ignoreMessage(QtDebugMsg, qPrintable(message.arg(m_repositoryDir)));
        message = "Update component \"C\" in \"%1\" .";
        QTest::ignoreMessage(QtDebugMsg, qPrintable(message.arg(m_repositoryDir)));
    }

private slots:
    void init()
    {
        ignoreMessageForCollectingPackages("1.0.0", "1.0.0");

        m_repositoryDir = QInstallerTools::makePathAbsolute(QInstaller::generateTemporaryFileName());
        m_tempDirDeleter.add(m_repositoryDir);
        generateTempMetaDir();

        m_packagesDirectory = ":///packages";

        ignoreMessagesForComponentHash(QStringList() << "A" << "B", false);
        ignoreMessagesForCopyMetadata("A", true, false); //Only A has metadata
        ignoreMessagesForCopyMetadata("B", false, false);
    }

    void initTestCase()
    {
        Lib7z::initSevenZ();
    }

    void testWithComponentMeta()
    {
        ignoreMessagesForComponentSha(QStringList () << "A" << "B", false);
        generateRepo(true, false, false);

        verifyComponentRepository("1.0.0", true);
        verifyComponentMetaUpdatesXml();
    }

    void testWithComponentAndUniteMeta()
    {
        ignoreMessagesForComponentSha(QStringList() << "A" << "B", false);
        ignoreMessagesForUniteMeta(false);
        generateRepo(true, true, false);

        verifyComponentRepository("1.0.0", true);
        verifyUniteMetadata("1.0.0");
    }

    void testWithUniteMeta()
    {
        ignoreMessagesForUniteMeta(false);
        generateRepo(false, true, false);

        verifyComponentRepository("1.0.0", false);
        verifyUniteMetadata("1.0.0");
    }

    void testUpdateNewComponents()
    {
        // Create 'base' repository which will be updated
        ignoreMessagesForComponentSha(QStringList() << "A" << "B", false);
        generateRepo(true, false, false);
        verifyComponentRepository("1.0.0", true);

        // Update the 'base' repository
        initRepoUpdate();
        ignoreMessageForCollectingPackages("2.0.0", "1.0.0");
        ignoreMessagesForComponentSha(QStringList() << "A", false); //Only A has update
        ignoreMessagesForComponentHash(QStringList() << "A", true);
        ignoreMessagesForCopyMetadata("A", true, true);
        const QString &message = "Update component \"A\" in \"%1\" .";
        QTest::ignoreMessage(QtDebugMsg, qPrintable(message.arg(m_repositoryDir)));
        generateRepo(true, false, true);
        verifyComponentRepository("2.0.0", true);
        verifyComponentMetaUpdatesXml();
    }

    void testUpdateComponents()
    {
        ignoreMessagesForComponentSha(QStringList() << "A" << "B", false);
        generateRepo(true, false, false);
        verifyComponentRepository("1.0.0",true);

        initRepoUpdate();
        ignoreMessagesForUpdateComponents();
        generateRepo(true, false, false);
        verifyComponentRepository("2.0.0", true);
        verifyComponentMetaUpdatesXml();
    }

    void testUpdateComponentsFromPartialPackageDir()
    {
        ignoreMessagesForComponentSha(QStringList() << "A" << "B", false);
        ignoreMessagesForUniteMeta(false);
        generateRepo(true, true, false);
        verifyComponentRepository("1.0.0",true);

        clearData();
        m_packagesDirectory = ":///packages_new";
        { // ignore messages
            ignoreMessagesForUniteMeta(false);
            ignoreMessageForCollectingPackages(QString(), QString(), "1.0.0");
            ignoreMessagesForComponentSha(QStringList() << "C", true);
            ignoreMessagesForCopyMetadata("C", false, false);
            ignoreMessagesForComponentHash(QStringList() << "C", false);
        }
        generateRepo(true, true, false);
        verifyComponentRepository("1.0.0", true);
        VerifyInstaller::verifyFileExistence(m_repositoryDir + "/C",
            QStringList() << "1.0.0content.7z" << "1.0.0content.7z.sha1" << "1.0.0meta.7z");
        verifyUniteMetadata("1.0.0");
    }

    void testUpdateComponentsWithUniteMetadata()
    {
        ignoreMessagesForComponentSha(QStringList() << "A" << "B", false);
        ignoreMessagesForUniteMeta(false);
        generateRepo(true, true, false);
        verifyComponentRepository("1.0.0", true);

        initRepoUpdate();
        ignoreMessagesForUpdateComponents();
        ignoreMessagesForUniteMeta(true);
        generateRepo(true, true, false);
        verifyComponentRepository("2.0.0", true);
        verifyUniteMetadata("2.0.0");
    }

    void testUpdateComponentsWithOnlyUniteMetadata()
    {
        ignoreMessagesForUniteMeta(false);
        generateRepo(false, true, false);
        verifyComponentRepository("1.0.0", false);

        initRepoUpdate();
        ignoreMessageForCollectingPackages("2.0.0", "1.0.0");
        ignoreMessagesForComponentHash(QStringList() << "A" << "B", true);
        ignoreMessagesForCopyMetadata("A", true, true);
        ignoreMessagesForCopyMetadata("B", false, true);
        ignoreMessagesForUniteMeta(true);
        generateRepo(false, true, false);
        verifyComponentRepository("2.0.0", false);
        verifyUniteMetadata("2.0.0");
    }

    void testUpdateComponentsFromRepository()
    {
        ignoreMessagesForComponentSha(QStringList() << "A" << "B", false);
        generateRepo(true, false, false);
        verifyComponentRepository("1.0.0", true);

        initRepoUpdateFromRepository(":///repository_component");
        ignoreMessageForCollectingRepository("repository_component");
        QTest::ignoreMessage(QtDebugMsg, "- it provides the package \"B\"  -  \"1.0.0\"");

        ignoreMessagesForCopyRepository("B", "1.0.0", "repository_component");
        ignoreMessagesForCopyRepository("A", "2.0.0", "repository_component");
        ignoreMessagesForComponentSha(QStringList() << "A" << "B", true);
        generateRepo(true, false, false);
        verifyComponentRepository("2.0.0", true);
        verifyComponentMetaUpdatesXml();
    }

    void testUpdateComponentsFromRepositoryWithUniteMetadata()
    {
        ignoreMessagesForComponentSha(QStringList() << "A" << "B", false);
        ignoreMessagesForUniteMeta(false);
        generateRepo(true, true, false);
        verifyComponentRepository("1.0.0", true);

        initRepoUpdateFromRepository(":///repository_componentAndUnite");
        ignoreMessageForCollectingRepository("repository_componentAndUnite");
        QTest::ignoreMessage(QtDebugMsg, "- it provides the package \"C\"  -  \"1.0.0\"");
        ignoreMessageForUpdateComponent();
        ignoreMessagesForCopyRepository("A", "2.0.0", "repository_componentAndUnite");
        ignoreMessagesForCopyRepository("C", "1.0.0", "repository_componentAndUnite");
        ignoreMessagesForUniteMeta(true);
        ignoreMessagesForComponentSha(QStringList() << "A" << "C", true);

        generateRepo(true, true, true);
        verifyComponentRepository("2.0.0", true);
        VerifyInstaller::verifyFileExistence(m_repositoryDir + "/C", QStringList() << "1.0.0content.7z" << "1.0.0content.7z.sha1" << "1.0.0meta.7z");
        verifyUniteMetadata("2.0.0");
    }

    void testUpdateComponentsFromRepositoryWithOnlyUniteMetadata()
    {
        ignoreMessagesForUniteMeta(false);
        generateRepo(false, true, false);
        verifyComponentRepository("1.0.0", false);

        initRepoUpdateFromRepository(":///repository_unite");
        ignoreMessageForCollectingRepository("repository_unite");
        QTest::ignoreMessage(QtDebugMsg, "- it provides the package \"C\"  -  \"1.0.0\"");
        ignoreMessageForUpdateComponent();
        ignoreMessagesForCopyRepository("A", "2.0.0", "repository_unite");
        ignoreMessagesForCopyRepository("C", "1.0.0", "repository_unite");
        ignoreMessagesForUniteMeta(true);

        generateRepo(false, true, true);
        verifyComponentRepository("2.0.0", false);
        VerifyInstaller::verifyFileExistence(m_repositoryDir + "/C", QStringList() << "1.0.0content.7z" << "1.0.0content.7z.sha1");
        verifyUniteMetadata("2.0.0");
    }

    void cleanup()
    {
        m_tempDirDeleter.releaseAndDeleteAll();
        m_packagesDirectory.clear();
        m_packages.clear();
        m_repositoryDirectories.clear();
    }

private:
    QString m_tmpMetaDir;
    QString m_repositoryDir;
    QString m_packagesDirectory;
    QStringList m_repositoryDirectories;
    QInstallerTools::PackageInfoVector m_packages;
    TempDirDeleter m_tempDirDeleter;
};

QTEST_MAIN(tst_repotest)

#include "tst_repotest.moc"
