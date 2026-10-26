#include "mainwindow.h"
#include "session_history.h"
#include <QAccessible>
#include <QComboBox>
#include <QDialog>
#include <QDir>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QSettings>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QtTest>

class WorkspaceTest : public QObject
{
    Q_OBJECT
    QTemporaryDir settingsDirectory;
  private slots:
    void initTestCase()
    {
        QVERIFY(settingsDirectory.isValid());
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settingsDirectory.path());
    }

    void stockEditCreatesCopyAndPreservesUndo()
    {
        MainWindow window(nullptr, true);
        window.show();
        window.activateWindow();
        QVERIFY(QTest::qWaitForWindowActive(&window));
        auto *editor = window.findChild<CurveEditor *>("curveEditor");
        QVERIFY(editor);
        auto *table = editor->findChild<QTableWidget *>("curvePointTable");
        auto *profiles = window.findChild<QListWidget *>("profiles");
        QVERIFY(editor && table && profiles);
        const auto original = editor->points();
        table->item(3, 1)->setText("66");
        QVERIFY(profiles->currentItem()->text().contains("Custom"));
        QCOMPARE(editor->points()[3].second, 66.0);
        QVERIFY(editor->canUndo());
        table->setFocus();
        QApplication::processEvents();
        QTest::keyClick(table, Qt::Key_Z, Qt::ControlModifier);
        QCOMPARE(editor->points(), original);
        QVERIFY(profiles->currentItem());
        QCOMPARE(profiles->currentItem()->text(), QString("Balanced"));
        QVERIFY(!window.findChild<QPushButton *>("applyChanges")->isEnabled());
    }

    void historyBelongsToDiagnostics()
    {
        MainWindow window(nullptr, true);
        auto *diagnostics = window.findChild<QWidget *>("diagnosticsPage");
        QVERIFY(diagnostics);
        QVERIFY(diagnostics->findChild<SessionHistory *>("sessionHistory"));
        QVERIFY(!window.findChild<QPushButton *>("openHistory"));
    }

    void selectionOnlyAppliesAfterSaveAndUse()
    {
        MainWindow window(nullptr, true);
        window.show();
        auto *profiles = window.findChild<QListWidget *>("profiles");
        QVERIFY(profiles);
        for (int row = 0; row < profiles->count(); ++row) {
            if (profiles->item(row)->data(Qt::UserRole).toString() == "Quiet") {
                QTest::mouseClick(profiles->viewport(), Qt::LeftButton, Qt::NoModifier,
                                  profiles->visualItemRect(profiles->item(row)).center());
                break;
            }
        }
        QTRY_COMPARE(profiles->currentItem()->text(), QString("Quiet"));
        QApplication::processEvents();
        QCOMPARE(window.findChild<QLabel *>("configuredProfile")->text(), QString("Balanced"));
        auto *save = window.findChild<QPushButton *>("applyChanges");
        QVERIFY(save->isEnabled());
        QVERIFY(save->text().contains("Quiet"));
        save->click();
        QCOMPARE(window.findChild<QLabel *>("configuredProfile")->text(), QString("Quiet"));
        QVERIFY(!save->isEnabled());
    }

    void createProfileExposesCustomActions()
    {
        MainWindow window(nullptr, true);
        window.show();
        QTimer::singleShot(0, &window, [] {
            auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
            if (!dialog)
                return;
            auto *name = dialog->findChild<QLineEdit *>("profileName");
            if (!name)
                return;
            name->setText("Study");
            dialog->accept();
        });
        window.findChild<QPushButton *>("createProfile")->click();
        auto *profiles = window.findChild<QListWidget *>("profiles");
        QVERIFY(profiles->currentItem());
        QCOMPARE(profiles->currentItem()->text(), QString("Study"));
        for (const auto *name : {"duplicateProfile", "renameProfile", "deleteProfile"}) {
            auto *button = window.findChild<QPushButton *>(name);
            QVERIFY(button && button->isVisible() && button->isEnabled());
        }
        QVERIFY(window.findChild<QPushButton *>("applyChanges")->text().contains("Study"));
        QCOMPARE(window.findChild<QLabel *>("configuredProfile")->text(), QString("Balanced"));
    }

    void capturesAreRealWidgets()
    {
        const QString output = qEnvironmentVariable("FANBOOST_CAPTURE_DIR");
        MainWindow window(nullptr, true);
        window.resize(1180, 780);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        auto *editor = window.findChild<CurveEditor *>("curveEditor");
        QVERIFY(editor);
        QVERIFY(!editor->accessibleName().isEmpty());
        auto *table = editor->findChild<QTableWidget *>("curvePointTable");
        QVERIFY(table);
        auto *accessible = QAccessible::queryAccessibleInterface(table);
        QVERIFY(accessible);
        QCOMPARE(accessible->role(), QAccessible::Table);
        if (output.isEmpty())
            return;
        QVERIFY(QDir().mkpath(output));
        auto *theme = window.findChild<QComboBox *>("appearanceTheme");
        QVERIFY(theme);
        theme->setCurrentIndex(theme->findData("light"));
        QTest::qWait(100);
        QVERIFY(window.grab().save(output + "/desktop.png"));
        theme->setCurrentIndex(theme->findData("dark"));
        QTest::qWait(100);
        QVERIFY(window.grab().save(output + "/dark.png"));
        theme->setCurrentIndex(theme->findData("light"));
        window.resize(800, 600);
        QTest::qWait(100);
        QVERIFY(window.grab().save(output + "/compact.png"));
        TelemetryData failed;
        failed.serviceKnown = true;
        failed.serviceFailed = true;
        failed.serviceAgeMs = 0;
        failed.driverLoaded = true;
        QVERIFY(QMetaObject::invokeMethod(&window, "onTelemetryUpdated", Qt::DirectConnection,
                                          Q_ARG(TelemetryData, failed)));
        QTest::qWait(100);
        QVERIFY(window.grab().save(output + "/failure.png"));
        const QFont originalFont = qApp->font();
        QFont enlarged = originalFont;
        enlarged.setPointSizeF(enlarged.pointSizeF() * 2.0);
        qApp->setFont(enlarged);
        MainWindow enlargedWindow(nullptr, true);
        enlargedWindow.resize(800, 600);
        enlargedWindow.show();
        QTest::qWait(100);
        QVERIFY(enlargedWindow.grab().save(output + "/large-text.png"));
        enlargedWindow.activateWindow();
        QVERIFY(QTest::qWaitForWindowActive(&enlargedWindow));
        auto *largeEditor = enlargedWindow.findChild<CurveEditor *>("curveEditor");
        QPushButton *addPoint = nullptr;
        for (auto *button : largeEditor->findChildren<QPushButton *>())
            if (button->accessibleName() == "Add curve point")
                addPoint = button;
        QVERIFY(addPoint);
        enlargedWindow.findChild<QListWidget *>("profiles")->setFocus();
        for (int step = 0; step < 100 && QApplication::focusWidget() != addPoint; ++step) {
            QTest::keyClick(QApplication::focusWidget(), Qt::Key_Tab);
            QApplication::processEvents();
        }
        QCOMPARE(QApplication::focusWidget(), addPoint);
        QVERIFY(!addPoint->visibleRegion().isEmpty());
        QVERIFY(enlargedWindow.grab().save(output + "/large-text-controls.png"));
        qApp->setFont(originalFont);
    }

    void liveHardwareReadings()
    {
        if (!qEnvironmentVariableIsSet("FANBOOST_LIVE_CHECK"))
            QSKIP("Live hardware check is opt-in.");
        TelemetryMonitor monitor;
        TelemetryData latest;
        connect(&monitor, &TelemetryMonitor::telemetryUpdated, this,
                [&latest](const TelemetryData &data) { latest = data; });
        monitor.start(300);
        QTRY_VERIFY_WITH_TIMEOUT(latest.tempValid && latest.targetValid && latest.serviceKnown, 5000);
        QVERIFY(latest.driverLoaded);
        qInfo("Live CPU %.1f C, fan request %d RPM, service %s", latest.tempCelsius, latest.targetRpm,
              latest.serviceActive ? "active" : "inactive");
        monitor.stop();
        const QString output = qEnvironmentVariable("FANBOOST_LIVE_CAPTURE_DIR");
        if (!output.isEmpty()) {
            MainWindow window(nullptr, false);
            window.show();
            QVERIFY(QTest::qWaitForWindowExposed(&window));
            QTest::qWait(1500);
            QVERIFY(QDir().mkpath(output));
            QVERIFY(window.grab().save(output + "/live-desktop.png"));
        }
    }
};

QTEST_MAIN(WorkspaceTest)
#include "test_workspace.moc"
