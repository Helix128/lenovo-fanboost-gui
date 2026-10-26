#pragma once

#include <QSystemTrayIcon>
#include <QMenu>
#include <QActionGroup>

class ConfigManager;
struct TelemetryData;

class TrayIcon : public QSystemTrayIcon {
    Q_OBJECT
public:
    explicit TrayIcon(ConfigManager *configMgr, QObject *parent = nullptr);
    ~TrayIcon() override = default;

    void updateTelemetry(const TelemetryData &data);
    void updateProfileList();

signals:
    void openWindowRequested();
    void profileChangeRequested(const QString &profileName);
    void serviceToggleRequested(bool activate);

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
};
