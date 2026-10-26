#include "session_history.h"

#include <QLabel>
#include <QTableWidget>
#include <QtTest>

class SessionHistoryTest : public QObject
{
    Q_OBJECT

  private slots:
    void keepsOnlyRecentSamples()
    {
        SessionHistory history;
        for (int i = 0; i < 601; ++i) {
            TelemetryData data;
            data.tempValid = true;
            data.tempCelsius = i;
            data.targetValid = true;
            data.targetRpm = i * 10;
            history.append(data);
        }
        QCOMPARE(history.sampleCount(), 600);
        auto *table = history.findChild<QTableWidget *>("sessionHistoryTable");
        QVERIFY(table);
        QCOMPARE(table->rowCount(), 600);
        QCOMPARE(table->item(0, 1)->text(), QString("1.0 °C"));
        QCOMPARE(table->item(599, 2)->text(), QString("6000 RPM"));
    }

    void invalidReadingsAreShownAsUnavailable()
    {
        SessionHistory history;
        TelemetryData data;
        data.tempValid = true;
        data.tempCelsius = 64.25;
        data.targetValid = false;
        history.append(data);

        auto *table = history.findChild<QTableWidget *>("sessionHistoryTable");
        QVERIFY(table);
        QCOMPARE(table->item(0, 1)->text(), QString("64.3 °C"));
        QCOMPARE(table->item(0, 2)->text(), QString("Unavailable"));
        QVERIFY(history.findChild<QLabel *>("sessionHistorySummary")->text().contains("current 64.3 °C"));

        data.tempValid = false;
        data.targetValid = true;
        data.targetRpm = 0;
        history.append(data);
        QCOMPARE(table->item(1, 1)->text(), QString("Unavailable"));
        QCOMPARE(table->item(1, 2)->text(), QString("0 RPM"));
        QVERIFY(history.findChild<QLabel *>("sessionHistorySummary")->text().contains("minimum 64.3 °C"));
    }

    void clearRemovesSamplesAndResetsSummary()
    {
        SessionHistory history;
        TelemetryData data;
        data.tempValid = true;
        data.tempCelsius = 50;
        history.append(data);
        history.clear();
        QCOMPARE(history.sampleCount(), 0);
        QCOMPARE(history.findChild<QTableWidget *>("sessionHistoryTable")->rowCount(), 0);
        QCOMPARE(history.findChild<QLabel *>("sessionHistorySummary")->text(),
                 QString("No telemetry samples yet."));
    }
};

QTEST_MAIN(SessionHistoryTest)
#include "test_session_history.moc"
