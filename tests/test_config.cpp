#include "config_manager.h"
#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

class ConfigTest : public QObject
{
    Q_OBJECT
  private slots:
    void profileNamesAndDecimalPointsRoundTrip()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString name = QStringLiteral("Compile [AC] \\\"quiet\\\" \\ profile");
        const CurvePoints points{{40, 0}, {60.5, 45.5}, {90, 100}};
        QMap<QString, FanProfile> profiles{{name, {name, points}}};
        const QString content = ConfigManager::serializeToToml(name, 0.75, profiles);
        QVERIFY(!content.isEmpty());
        QFile file(directory.filePath("profiles.toml"));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(content.toUtf8());
        file.close();
        ConfigManager config;
        QVERIFY(config.load(file.fileName()));
        QCOMPARE(config.activeProfile(), name);
        QCOMPARE(config.profile(name).points, points);
        QCOMPARE(config.interval(), 0.75);
    }

    void invalidCurvesCannotBeSerialized()
    {
        const QList<CurvePoints> invalid{{{40, 0}},
                                         {{40, 0}, {40, 100}},
                                         {{40, 20}, {60, 10}},
                                         {{40, 0}, {60, 101}},
                                         {{29, 0}, {90, 100}}};
        for (const auto &points : invalid) {
            QVERIFY(ConfigManager::serializeToToml("Test", 0.5, {{"Test", {"Test", points}}}).isEmpty());
        }
        QVERIFY(ConfigManager::serializeToToml("Missing", 0.5, {}).isEmpty());
    }

    void malformedConfigCannotOverwriteExistingFile()
    {
        QTemporaryDir directory;
        QFile file(directory.filePath("broken.toml"));
        QVERIFY(file.open(QIODevice::WriteOnly));
        const QByteArray broken("[fanboost]\nprofile = \"Balanced\"\nunknown_setting = true\n");
        file.write(broken);
        file.close();
        ConfigManager config;
        QVERIFY(!config.load(file.fileName()));
        QSignalSpy result(&config, &ConfigManager::saveFinished);
        config.saveAsync(file.fileName());
        QCOMPARE(result.count(), 1);
        QVERIFY(!result.first().first().toBool());
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(file.readAll(), broken);
    }

    void externalChangesAreDetectedBeforeAuthorization()
    {
        QTemporaryDir directory;
        QFile file(directory.filePath("profiles.toml"));
        const auto content =
            ConfigManager::serializeToToml(
                "Balanced", 0.5, {{"Balanced", {"Balanced", ConfigManager::defaultBalancedPoints()}}})
                .toUtf8();
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(content);
        file.close();
        ConfigManager config;
        QVERIFY(config.load(file.fileName()));
        QVERIFY(file.open(QIODevice::Append));
        file.write("# External change\n");
        file.close();
        QSignalSpy result(&config, &ConfigManager::saveFinished);
        config.saveAsync(file.fileName());
        QCOMPARE(result.count(), 1);
        QVERIFY(!result.first().first().toBool());
        QVERIFY(!config.isBusy());
    }

    void authorizedSave_data()
    {
        QTest::addColumn<bool>("conflict");
        QTest::addColumn<bool>("restartFails");
        QTest::newRow("atomic-save") << false << false;
        QTest::newRow("change-during-authorization") << true << false;
        QTest::newRow("restart-failure-restores-config") << false << true;
    }

    void authorizedSave()
    {
        QFETCH(bool, conflict);
        QFETCH(bool, restartFails);
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto original =
            ConfigManager::serializeToToml(
                "Balanced", 0.5, {{"Balanced", {"Balanced", ConfigManager::defaultBalancedPoints()}}})
                .toUtf8();
        QFile configFile(directory.filePath("profiles.toml"));
        QVERIFY(configFile.open(QIODevice::WriteOnly));
        configFile.write(original);
        configFile.close();
        auto script = [&directory](const QString &name, const QByteArray &body) {
            QFile file(directory.filePath(name));
            if (!file.open(QIODevice::WriteOnly))
                return false;
            file.write("#!/bin/sh\n" + body);
            file.close();
            return file.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
        };
        QVERIFY(script("systemctl",
                       restartFails
                           ? QByteArray("if [ \"$1\" = show ]; then printf "
                                        "'LoadState=loaded\\nActiveState=active\\n'; else exit 1; fi\n")
                           : QByteArray("printf 'LoadState=loaded\\nActiveState=inactive\\n'\n")));
        QVERIFY(script("pkexec",
                       conflict
                           ? QByteArray("printf '# changed during authorization\\n' >> \"$5\"\nexec \"$@\"\n")
                           : QByteArray("exec \"$@\"\n")));
        struct RestorePath {
            QByteArray value = qgetenv("PATH");
            ~RestorePath() { qputenv("PATH", value); }
        } restore;
        qputenv("PATH", directory.path().toUtf8() + ':' + restore.value);
        ConfigManager config;
        QVERIFY(config.load(configFile.fileName()));
        config.setInterval(0.75);
        QSignalSpy result(&config, &ConfigManager::saveFinished);
        config.saveAsync(configFile.fileName());
        QTRY_COMPARE_WITH_TIMEOUT(result.count(), 1, 3000);
        QCOMPARE(result.first().first().toBool(), !conflict && !restartFails);
        QVERIFY(configFile.open(QIODevice::ReadOnly));
        const auto saved = configFile.readAll();
        if (conflict)
            QCOMPARE(saved, original + "# changed during authorization\n");
        else if (restartFails)
            QCOMPARE(saved, original);
        else
            QVERIFY(saved.contains("interval = 0.75"));
    }
};

QTEST_MAIN(ConfigTest)
#include "test_config.moc"
