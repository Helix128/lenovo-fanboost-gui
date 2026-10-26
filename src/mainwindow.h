#pragma once

#include "config_manager.h"
#include "curve_editor.h"
#include "telemetry_monitor.h"
#include "tray_icon.h"
#include <QByteArray>
#include <QCloseEvent>
#include <QElapsedTimer>
#include <QMainWindow>
#include <QSettings>

class QLabel;
class QPushButton;
class QListWidget;
class QStackedWidget;
class QCheckBox;
class QComboBox;
class QLineEdit;
class SessionHistory;
class QGridLayout;
class QDialog;
class QDialogButtonBox;
class QLineEdit;

class MainWindow : public QMainWindow
{
    Q_OBJECT
  public:
    explicit MainWindow(QWidget *parent = nullptr, bool previewMode = false);
    ~MainWindow() override = default;

  protected:
    void closeEvent(QCloseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

  private slots:
    void onTelemetryUpdated(const TelemetryData &data);

  private:
    void onConfigLoaded();
    void onCurveModified(const CurvePoints &points);
    void onSaveClicked();
    void onDiscardClicked();
    void onModeSelected(int index);
    void setupUi();
    void updateTelemetryUi(const TelemetryData &data);
    void updateProfileListUi();
    void setModified(bool modified);
    bool resolveDraft(const QString &nextProfile = {});
    void applyChanges();
    void showPage(int index);
    void applyTheme(const QString &theme);
    void updateStateLayout();
    void updateBusyState();
    QString currentProfileName() const;
    QByteArray configFingerprint() const;
    void selectProfile(const QString &name);
    bool hasProfileEdits() const;
    void updateProfileActions();
    void createProfile();
    void duplicateProfile();
    void renameProfile();
    void deleteProfile();
    void requestMode(int mode, bool explicitProfileUse = false);
    void syncModeSelector();
    void updateActionLabel();
    void setupSessionTools();
    void showStatus(const QString &text);

    ConfigManager m_configMgr;
    TelemetryMonitor m_telemetry;
    TrayIcon *m_trayIcon{nullptr};
    const bool m_previewMode;
    QSettings m_settings;
    QString m_configuredProfile;
    QString m_previewProfile;
    QString m_pendingProfile;
    QString m_savingProfile;
    QString m_implicitCopySource;
    QString m_implicitCopyName;
    QByteArray m_savedFingerprint;
    QString m_theme{"system"};

    QLabel *m_serviceValue{nullptr};
    QLabel *m_tempValue{nullptr};
    QLabel *m_targetValue{nullptr};
    QLabel *m_configuredValue{nullptr};
    QComboBox *m_modeSelector{nullptr};
    QWidget *m_modeControls{nullptr};
    QPushButton *m_refreshStatusButton{nullptr};
    QListWidget *m_profiles{nullptr};
    QPushButton *m_createProfileButton{nullptr};
    QPushButton *m_importProfileButton{nullptr};
    QWidget *m_profileActions{nullptr};
    QPushButton *m_duplicateProfileButton{nullptr};
    QPushButton *m_renameProfileButton{nullptr};
    QPushButton *m_deleteProfileButton{nullptr};
    QPushButton *m_exportProfileButton{nullptr};
    QLabel *m_editorTitle{nullptr};
    QLabel *m_editorSubTitle{nullptr};
    CurveEditor *m_curveEditor{nullptr};
    QStackedWidget *m_pages{nullptr};
    QLabel *m_statusMessage{nullptr};
    QPushButton *m_applyButton{nullptr};
    QPushButton *m_discardButton{nullptr};
    QCheckBox *m_closeToTray{nullptr};
    QComboBox *m_themeCombo{nullptr};
    QLabel *m_diagnosticsText{nullptr};
    QLabel *m_bootStatus{nullptr};
    bool m_isModified{false};
    bool m_currentServiceActive{false};
    bool m_serviceKnown{false};
    bool m_modePending{false};
    bool m_modeOperationPending{false};
    bool m_startServiceAfterSave{false};
    int m_pendingMode{-1};
    int m_confirmedMode{-1};
    bool m_operationBusy{false};
    bool m_closingAfterApply{false};
    bool m_savedCloseToTray{true};
    int m_currentMaxRpm{0};
    SessionHistory *m_history{nullptr};
    QElapsedTimer m_historyClock;
    QGridLayout *m_headingLayout{nullptr};
    QWidget *m_headingNames{nullptr};
    QWidget *m_headingActions{nullptr};
    QGridLayout *m_readingsLayout{nullptr};
    QLabel *m_cpuCaption{nullptr};
    QLabel *m_requestCaption{nullptr};
};
