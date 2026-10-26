#include "autostart.h"
#include "profile_io.h"
#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

class FeaturesTest : public QObject
{
    Q_OBJECT
  private slots:
    void profileExchange()
    {
        QTemporaryDir directory;
        const auto path = directory.filePath("profile.json");
        const FanProfile original{"Compile (AC)", {{40, 0}, {60.5, 45.5}, {90, 100}}};
        QString error;
        QVERIFY2(ProfileIO::write(path, original, &error), qPrintable(error));
        FanProfile loaded;
        QVERIFY2(ProfileIO::read(path, &loaded, &error), qPrintable(error));
        QCOMPARE(loaded.name, original.name);
        QCOMPARE(loaded.points, original.points);
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("{\"version\":2,\"name\":\"Bad\",\"points\":[]}");
        file.close();
        QVERIFY(!ProfileIO::read(path, &loaded, &error));
        QCOMPARE(loaded.name, original.name);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(QByteArray(65537, 'x'));
        file.close();
        QVERIFY(!ProfileIO::read(path, &loaded, &error));
    }
    void loginStartupIsSeparateAndReversible()
    {
        QTemporaryDir directory;
        const auto previous = qgetenv("XDG_CONFIG_HOME");
        qputenv("XDG_CONFIG_HOME", directory.path().toUtf8());
        QString error;
        const bool initiallyDisabled = !Autostart::enabled();
        const bool created = Autostart::setEnabled(true, &error);
        const bool enabled = Autostart::enabled();
        const bool removed = Autostart::setEnabled(false, &error);
        const bool disabled = !Autostart::enabled();
        if (previous.isNull())
            qunsetenv("XDG_CONFIG_HOME");
        else
            qputenv("XDG_CONFIG_HOME", previous);
        QVERIFY(initiallyDisabled && created && enabled && removed && disabled);
    }
};
QTEST_MAIN(FeaturesTest)
#include "test_features.moc"
