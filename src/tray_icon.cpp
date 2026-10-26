#include "tray_icon.h"
#include "config_manager.h"
#include "telemetry_monitor.h"

#include <QApplication>
#include <QPainter>
#include <QPixmap>

TrayIcon::TrayIcon(ConfigManager *configMgr, QObject *parent)
    : QSystemTrayIcon(parent), m_configMgr(configMgr)
{
    setIcon(QIcon(QStringLiteral(":/icons/lenovo-fanboost-gui-64.png")));
    buildMenu();
    connect(this, &QSystemTrayIcon::activated, this, &TrayIcon::onActivated);
    if (m_configMgr) {
        connect(m_configMgr, &ConfigManager::configLoaded, this, &TrayIcon::updateProfileList);
        connect(m_configMgr, &ConfigManager::activeProfileChanged, this, &TrayIcon::updateProfileList);
    }
}

void TrayIcon::buildMenu()
{
    m_menu = new QMenu;
    m_stateHeaderAct = m_menu->addAction(tr("Lenovo Fan Boost (status unknown)"));
    QFont font = m_stateHeaderAct->font();
    font.setBold(true);
    m_stateHeaderAct->setFont(font);
    m_stateHeaderAct->setEnabled(false);
    m_stateToggleAct = m_menu->addAction(tr("Start fan control"));
    connect(m_stateToggleAct, &QAction::triggered, this,
            [this] { emit serviceToggleRequested(!m_lastActive); });
    m_menu->addSeparator();
    m_profileSubMenu = m_menu->addMenu(tr("Configured profile"));
    m_profileGroup = new QActionGroup(this);
    m_profileGroup->setExclusive(true);
    updateProfileList();
    m_menu->addSeparator();
    QAction *open = m_menu->addAction(tr("Open Fan Control"));
    connect(open, &QAction::triggered, this, &TrayIcon::openWindowRequested);
    QAction *quit = m_menu->addAction(tr("Quit"));
    connect(quit, &QAction::triggered, this, &TrayIcon::quitRequested);
    setContextMenu(m_menu);
}

void TrayIcon::updateProfileList()
{
    if (!m_profileSubMenu || !m_configMgr)
        return;
    m_profileSubMenu->clear();
    for (QAction *action : m_profileGroup->actions())
        m_profileGroup->removeAction(action);
    for (const QString &name : m_configMgr->profileNames()) {
        auto *action = m_profileSubMenu->addAction(name);
        action->setCheckable(true);
        action->setData(name);
        action->setChecked(name == m_configuredProfile);
        m_profileGroup->addAction(action);
        connect(action, &QAction::triggered, this, [this, name] { emit profileChangeRequested(name); });
    }
}

void TrayIcon::setOperationsEnabled(bool enabled)
{
    m_operationsEnabled = enabled;
    if (m_stateToggleAct)
        m_stateToggleAct->setEnabled(enabled && m_serviceKnown);
    if (m_profileGroup)
        for (QAction *action : m_profileGroup->actions())
            action->setEnabled(enabled);
}

void TrayIcon::setConfiguredProfile(const QString &name)
{
    if (m_configuredProfile == name)
        return;
    m_configuredProfile = name;
    updateProfileList();
}

void TrayIcon::updateTelemetry(const TelemetryData &data)
{
    m_lastTemp = data.tempCelsius;
    m_lastActive = data.serviceActive;
    m_serviceKnown = data.serviceKnown && data.serviceAgeMs >= 0 && data.serviceAgeMs <= 6000;
    m_tempValid = data.tempValid;
    m_targetValid = data.targetValid;
    m_stateToggleAct->setEnabled(m_operationsEnabled && m_serviceKnown);
    const bool serviceStale = data.serviceKnown && (data.serviceAgeMs < 0 || data.serviceAgeMs > 6000);
    const QString state = !data.serviceKnown   ? tr("status unknown")
                          : serviceStale       ? tr("service status stale")
                          : data.serviceActive ? tr("service running")
                          : data.serviceFailed ? tr("service failed")
                                               : tr("service stopped");
    m_stateHeaderAct->setText(tr("Lenovo Fan Boost (%1)").arg(state));
    m_stateToggleAct->setText(data.serviceActive ? tr("Use EC automatic") : tr("Start fan control"));
    const QString temp =
        data.tempValid ? QString::number(data.tempCelsius, 'f', 1) + tr(" °C") : tr("unavailable");
    const QString target = !data.targetValid     ? tr("unavailable")
                           : data.targetRpm == 0 ? tr("0 RPM · EC automatic")
                                                 : tr("%1 RPM").arg(data.targetRpm);
    const QString profile = m_configuredProfile.isEmpty() ? tr("unknown") : m_configuredProfile;
    setToolTip(tr("Lenovo Fan Boost\n%1\nCPU temperature: %2\nFan request: %3\nConfigured profile: %4")
                   .arg(state, temp, target, profile));
    const int roundedTemperature = data.tempValid ? qRound(data.tempCelsius) : -1;
    if (roundedTemperature != m_renderedTemperature || data.serviceActive != m_renderedActive) {
        setIcon(renderDynamicIcon(roundedTemperature, data.serviceActive));
        m_renderedTemperature = roundedTemperature;
        m_renderedActive = data.serviceActive;
    }
}

QIcon TrayIcon::renderDynamicIcon(double tempCelsius, bool isActive)
{
    QPixmap base(QStringLiteral(":/icons/lenovo-fanboost-gui-64.png"));
    if (base.isNull()) {
        base = QPixmap(64, 64);
        base.fill(Qt::transparent);
    }
    QPixmap result(64, 64);
    result.fill(Qt::transparent);
    QPainter p(&result);
    p.setRenderHint(QPainter::Antialiasing);
    if (!isActive)
        p.setOpacity(0.65);
    p.drawPixmap(0, 0, base);
    p.setOpacity(1);
    if (tempCelsius >= 0) {
        QColor color = tempCelsius >= 80   ? QColor("#c8102e")
                       : tempCelsius >= 65 ? QColor("#8a4b08")
                                           : QColor("#287a48");
        if (!isActive)
            color = QColor("#62666b");
        QRect badge(33, 39, 29, 23);
        p.setPen(QPen(Qt::white, 1.5));
        p.setBrush(color);
        p.drawRoundedRect(badge, 4, 4);
        p.setPen(Qt::white);
        QFont f = p.font();
        f.setBold(true);
        f.setPixelSize(12);
        p.setFont(f);
        p.drawText(badge, Qt::AlignCenter, QString::number(qRound(tempCelsius)));
    }
    return QIcon(result);
}

void TrayIcon::onActivated(QSystemTrayIcon::ActivationReason reason)
{
    if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick)
        emit openWindowRequested();
}
