#pragma once

#include <QMainWindow>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QCheckBox>
#include <QCloseEvent>

#include "config_manager.h"
#include "telemetry_monitor.h"
#include "curve_editor.h"
#include "tray_icon.h"

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override = default;

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void onTelemetryUpdated(const TelemetryData &data);
    void onConfigLoaded();
    void onProfileSelected(int index);
    void onCurveModified(const CurvePoints &points);
    void onSaveProfileClicked();
    void onRenameProfileClicked();
    void onNewProfileClicked();
    void onDeleteProfileClicked();
    void onSaveClicked();
    void onDiscardClicked();
    void onToggleServiceClicked();

private:
    void setupUi();
    void updateTelemetryUi(const TelemetryData &data);
    void updateProfileListUi();
    void setModified(bool modified);

    ConfigManager m_configMgr;
    TelemetryMonitor m_telemetry;
    TrayIcon *m_trayIcon{nullptr};

    // Telemetry UI widgets
    QLabel *m_stateBadge{nullptr};
    QPushButton *m_serviceToggleBtn{nullptr};
    QLabel *m_tempLabel{nullptr};
    QLabel *m_targetLabel{nullptr};
    QLabel *m_ecSpeedLabel{nullptr};
    QLabel *m_maxRpmLabel{nullptr};

    // Profile & Curve widgets
    QComboBox *m_profileCombo{nullptr};
    QPushButton *m_saveProfileBtn{nullptr};
    QPushButton *m_renameProfileBtn{nullptr};
    QPushButton *m_newProfileBtn{nullptr};
    QPushButton *m_deleteProfileBtn{nullptr};
    CurveEditor *m_curveEditor{nullptr};

    // Action bar widgets
    QLabel *m_statusMessage{nullptr};
    QPushButton *m_saveBtn{nullptr};
    QPushButton *m_discardBtn{nullptr};
    QCheckBox *m_closeToTrayCheck{nullptr};

    bool m_isModified{false};
    bool m_currentServiceActive{false};
    int m_currentMaxRpm{4000};
};
