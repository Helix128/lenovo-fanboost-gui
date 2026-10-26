#include "tray_icon.h"
#include "config_manager.h"
#include "telemetry_monitor.h"

#include <QApplication>
#include <QPainter>
#include <QPixmap>

TrayIcon::TrayIcon(ConfigManager *configMgr, QObject *parent)
    : QSystemTrayIcon(parent)
    , m_configMgr(configMgr)
{
    setIcon(QIcon(":/icons/lenovo-fanboost-gui-64.png"));
    buildMenu();

    connect(this, &QSystemTrayIcon::activated, this, &TrayIcon::onActivated);
    if (m_configMgr) {
        connect(m_configMgr, &ConfigManager::configLoaded, this, &TrayIcon::updateProfileList);
        connect(m_configMgr, &ConfigManager::activeProfileChanged, this, [this](const QString &name) {
            if (m_profileGroup) {
                for (QAction *act : m_profileGroup->actions()) {
                    if (act->text() == name) {
                        act->setChecked(true);
                        break;
                    }
                }
            }
        });
    }
}

void TrayIcon::buildMenu()
{
    if (m_menu) {
        delete m_menu;
    }
    m_menu = new QMenu();

    m_stateHeaderAct = m_menu->addAction(tr("Lenovo Fan Boost"));
    QFont headerFont = m_stateHeaderAct->font();
    headerFont.setBold(true);
    m_stateHeaderAct->setFont(headerFont);
    m_stateHeaderAct->setEnabled(false);

    m_stateToggleAct = m_menu->addAction(tr("Switch to EC Mode"));
    connect(m_stateToggleAct, &QAction::triggered, this, [this]() {
        emit serviceToggleRequested(!m_lastActive);
    });

    m_menu->addSeparator();

    m_profileSubMenu = m_menu->addMenu(tr("Curve Profile"));
    m_profileGroup = new QActionGroup(this);
    m_profileGroup->setExclusive(true);
    updateProfileList();

    m_menu->addSeparator();

    QAction *openAct = m_menu->addAction(tr("Open Fan Control"));
    connect(openAct, &QAction::triggered, this, &TrayIcon::openWindowRequested);

    QAction *quitAct = m_menu->addAction(tr("Quit"));
    connect(quitAct, &QAction::triggered, qApp, &QCoreApplication::quit);

    setContextMenu(m_menu);
}

void TrayIcon::updateProfileList()
{
    if (!m_profileSubMenu || !m_configMgr) return;

    m_profileSubMenu->clear();
    const QStringList names = m_configMgr->profileNames();
    const QString active = m_configMgr->activeProfile();

    for (const QString &name : names) {
        QAction *act = m_profileSubMenu->addAction(name);
        act->setCheckable(true);
        if (name == active) {
            act->setChecked(true);
        }
        m_profileGroup->addAction(act);

        connect(act, &QAction::triggered, this, [this, name]() {
            emit profileChangeRequested(name);
        });
    }
}

void TrayIcon::updateTelemetry(const TelemetryData &data)
{
    m_lastTemp = data.tempCelsius;
    m_lastActive = data.serviceActive;

    QString stateStr = data.serviceActive ? tr("Active") : tr("Inactive (EC Mode)");
    m_stateHeaderAct->setText(QString("Lenovo Fan Boost — %1").arg(stateStr));
    m_stateToggleAct->setText(data.serviceActive ? tr("Switch to EC Mode") : tr("Activate Fan Boost"));

    // Tooltip
    QString tip = QString("Lenovo Fan Boost\nState: %1\nCPU: %2 °C\nTarget: %3 RPM\nProfile: %4")
                      .arg(stateStr)
                      .arg(data.tempCelsius, 0, 'f', 1)
                      .arg(data.targetRpm)
                      .arg(m_configMgr ? m_configMgr->activeProfile() : "Balanced");
    setToolTip(tip);

    setIcon(renderDynamicIcon(data.tempCelsius, data.serviceActive));
}

QIcon TrayIcon::renderDynamicIcon(double tempCelsius, bool isActive)
{
    QPixmap basePix(":/icons/lenovo-fanboost-gui-64.png");
    if (basePix.isNull()) {
        basePix = QPixmap(64, 64);
        basePix.fill(Qt::transparent);
    }

    QPixmap result(64, 64);
    result.fill(Qt::transparent);

    QPainter p(&result);
    p.setRenderHint(QPainter::Antialiasing, true);

    if (!isActive) {
        // Dim the icon slightly when in EC Mode
        p.setOpacity(0.65);
    }
    p.drawPixmap(0, 0, basePix);
    p.setOpacity(1.0);

    // Temperature badge in corner
    if (tempCelsius > 0.0) {
        QColor badgeColor = (tempCelsius >= 80.0) ? QColor("#da4453") :
                            (tempCelsius >= 65.0) ? QColor("#f67400") : QColor("#27ae60");
        if (!isActive) {
            badgeColor = QColor("#7f8c8d");
        }

        QRect badgeRect(32, 38, 30, 24);
        p.setPen(QPen(Qt::white, 1.5));
        p.setBrush(badgeColor);
        p.drawRoundedRect(badgeRect, 5, 5);

        p.setPen(Qt::white);
        QFont f = p.font();
        f.setBold(true);
        f.setPixelSize(12);
        p.setFont(f);
        p.drawText(badgeRect, Qt::AlignCenter, QString::number(std::round(tempCelsius)));
    }

    return QIcon(result);
}

void TrayIcon::onActivated(QSystemTrayIcon::ActivationReason reason)
{
    if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick) {
        emit openWindowRequested();
    }
}
