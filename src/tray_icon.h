#pragma once

#include <QActionGroup>
#include <QMenu>
#include <QSystemTrayIcon>

class ConfigManager;
struct TelemetryData;

class TrayIcon : public QSystemTrayIcon
{
    Q_OBJECT
  public:
    explicit TrayIcon(ConfigManager *configMgr, QObject *parent = nullptr);
    ~TrayIcon() override { delete m_menu; }

    void updateTelemetry(const TelemetryData &data);
    void updateProfileList();
    void setOperationsEnabled(bool enabled);
    void setConfiguredProfile(const QString &name);

  signals:
    void openWindowRequested();
    void profileChangeRequested(const QString &profileName);
    void serviceToggleRequested(bool activate);
    void quitRequested();

  private slots:
    void onActivated(QSystemTrayIcon::ActivationReason reason);

  private:
    void buildMenu();
    QIcon renderDynamicIcon(double tempCelsius, bool isActive);

    ConfigManager *m_configMgr{nullptr};
    QMenu *m_menu{nullptr};
    QAction *m_stateHeaderAct{nullptr};
    QAction *m_stateToggleAct{nullptr};
    QMenu *m_profileSubMenu{nullptr};
    QActionGroup *m_profileGroup{nullptr};

    double m_lastTemp{0.0};
    bool m_lastActive{false};
    bool m_serviceKnown{false};
    bool m_tempValid{false};
    bool m_targetValid{false};
    bool m_operationsEnabled{true};
    QString m_configuredProfile;
    int m_renderedTemperature{-999};
    bool m_renderedActive{false};
};
