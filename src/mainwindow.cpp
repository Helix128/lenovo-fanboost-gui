#include "mainwindow.h"

#include <QApplication>
#include <QBoxLayout>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QInputDialog>
#include <QMessageBox>
#include <QStandardItemModel>
#include <QStyle>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(tr("Lenovo Fan Boost"));
    setWindowIcon(QIcon(":/icons/lenovo-fanboost-gui-128.png"));
    resize(740, 560);
    setMinimumSize(640, 480);

    m_trayIcon = new TrayIcon(&m_configMgr, this);

    setupUi();

    connect(&m_telemetry, &TelemetryMonitor::telemetryUpdated, this, &MainWindow::onTelemetryUpdated);
    connect(&m_configMgr, &ConfigManager::configLoaded, this, &MainWindow::onConfigLoaded);
    connect(&m_configMgr, &ConfigManager::activeProfileChanged, this, [this](const QString &name) {
        int idx = m_profileCombo->findText(name);
        if (idx >= 0 && m_profileCombo->currentIndex() != idx) {
            m_profileCombo->setCurrentIndex(idx);
        }
    });

    connect(m_trayIcon, &TrayIcon::openWindowRequested, this, [this]() {
        showNormal();
        activateWindow();
        raise();
    });

    connect(m_trayIcon, &TrayIcon::profileChangeRequested, this, [this](const QString &name) {
        int idx = m_profileCombo->findText(name);
        if (idx >= 0) {
            m_profileCombo->setCurrentIndex(idx);
            m_configMgr.setActiveProfile(name);
            onSaveClicked();
        }
    });

    connect(m_trayIcon, &TrayIcon::serviceToggleRequested, this, [this](bool activate) {
        QString err;
        if (!TelemetryMonitor::setServiceActive(activate, &err)) {
            QMessageBox::warning(this, tr("Service Error"), err);
        }
        m_telemetry.refreshNow();
    });

    m_configMgr.load();
    m_telemetry.start(1000);
    m_trayIcon->show();
}

void MainWindow::setupUi()
{
    QWidget *central = new QWidget(this);
    setCentralWidget(central);
    auto *mainLayout = new QVBoxLayout(central);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    mainLayout->setSpacing(14);

    // ================= 1. TELEMETRY HEADER =================
    auto *headerFrame = new QFrame(this);
    headerFrame->setFrameShape(QFrame::StyledPanel);
    headerFrame->setStyleSheet("QFrame { border-radius: 6px; }");
    auto *headerLayout = new QGridLayout(headerFrame);
    headerLayout->setContentsMargins(14, 12, 14, 12);
    headerLayout->setHorizontalSpacing(20);
    headerLayout->setVerticalSpacing(8);

    // State Column
    auto *stateTitle = new QLabel(tr("Service State"), headerFrame);
    stateTitle->setStyleSheet("font-size: 11px; color: gray; font-weight: bold;");
    headerLayout->addWidget(stateTitle, 0, 0);

    auto *stateBox = new QHBoxLayout();
    m_stateBadge = new QLabel(tr("Active"), headerFrame);
    m_stateBadge->setStyleSheet("background-color: #27ae60; color: white; padding: 3px 10px; "
                                "border-radius: 4px; font-weight: bold; font-size: 12px;");
    m_serviceToggleBtn = new QPushButton(tr("Switch to EC Mode"), headerFrame);
    connect(m_serviceToggleBtn, &QPushButton::clicked, this, &MainWindow::onToggleServiceClicked);

    stateBox->addWidget(m_stateBadge);
    stateBox->addWidget(m_serviceToggleBtn);
    stateBox->addStretch();
    headerLayout->addLayout(stateBox, 1, 0);

    // CPU Temperature Column
    auto *tempTitle = new QLabel(tr("CPU Temperature"), headerFrame);
    tempTitle->setStyleSheet("font-size: 11px; color: gray; font-weight: bold;");
    headerLayout->addWidget(tempTitle, 0, 1);

    m_tempLabel = new QLabel("--.- °C", headerFrame);
    m_tempLabel->setStyleSheet("font-size: 18px; font-weight: bold;");
    headerLayout->addWidget(m_tempLabel, 1, 1);

    // Fan Target RPM Column
    auto *targetTitle = new QLabel(tr("Target Fan Speed"), headerFrame);
    targetTitle->setStyleSheet("font-size: 11px; color: gray; font-weight: bold;");
    headerLayout->addWidget(targetTitle, 0, 2);

    m_targetLabel = new QLabel("0 RPM (0%)", headerFrame);
    m_targetLabel->setStyleSheet("font-size: 18px; font-weight: bold;");
    headerLayout->addWidget(m_targetLabel, 1, 2);

    // Hardware Details Column
    auto *hwTitle = new QLabel(tr("Hardware Range & EC"), headerFrame);
    hwTitle->setStyleSheet("font-size: 11px; color: gray; font-weight: bold;");
    headerLayout->addWidget(hwTitle, 0, 3);

    auto *hwBox = new QVBoxLayout();
    m_maxRpmLabel = new QLabel(tr("Max: 4000 RPM"), headerFrame);
    m_maxRpmLabel->setStyleSheet("font-size: 12px;");
    m_ecSpeedLabel = new QLabel(tr("EC Speed: --"), headerFrame);
    m_ecSpeedLabel->setStyleSheet("font-size: 11px; color: gray;");
    hwBox->addWidget(m_maxRpmLabel);
    hwBox->addWidget(m_ecSpeedLabel);
    headerLayout->addLayout(hwBox, 1, 3);

    mainLayout->addWidget(headerFrame);

    // ================= 2. PROFILE BAR & CURVE EDITOR =================
    auto *profileBar = new QHBoxLayout();
    profileBar->setSpacing(8);

    auto *profileLabel = new QLabel(tr("Profile:"), this);
    profileLabel->setStyleSheet("font-weight: bold;");
    profileBar->addWidget(profileLabel);

    m_profileCombo = new QComboBox(this);
    m_profileCombo->setMinimumWidth(180);
    connect(m_profileCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::onProfileSelected);
    profileBar->addWidget(m_profileCombo);

    m_saveProfileBtn = new QPushButton(tr("Save Profile"), this);
    connect(m_saveProfileBtn, &QPushButton::clicked, this, &MainWindow::onSaveProfileClicked);
    profileBar->addWidget(m_saveProfileBtn);

    m_renameProfileBtn = new QPushButton(tr("Rename..."), this);
    connect(m_renameProfileBtn, &QPushButton::clicked, this, &MainWindow::onRenameProfileClicked);
    profileBar->addWidget(m_renameProfileBtn);

    m_newProfileBtn = new QPushButton(tr("New..."), this);
    connect(m_newProfileBtn, &QPushButton::clicked, this, &MainWindow::onNewProfileClicked);
    profileBar->addWidget(m_newProfileBtn);

    m_deleteProfileBtn = new QPushButton(tr("Delete"), this);
    connect(m_deleteProfileBtn, &QPushButton::clicked, this, &MainWindow::onDeleteProfileClicked);
    profileBar->addWidget(m_deleteProfileBtn);

    profileBar->addStretch();
    mainLayout->addLayout(profileBar);

    // Curve Editor Widget
    m_curveEditor = new CurveEditor(this);
    connect(m_curveEditor, &CurveEditor::curveChanged, this, &MainWindow::onCurveModified);
    mainLayout->addWidget(m_curveEditor, 1);

    auto *helpText = new QLabel(tr("Drag points to shape the curve. Double-click on the graph to add a point; right-click a point to delete it."), this);
    helpText->setStyleSheet("font-size: 11px; color: gray;");
    mainLayout->addWidget(helpText);

    // ================= 3. BOTTOM ACTION BAR =================
    auto *bottomBar = new QHBoxLayout();
    m_statusMessage = new QLabel(tr("Ready"), this);
    m_statusMessage->setStyleSheet("font-size: 11px; color: gray;");
    bottomBar->addWidget(m_statusMessage);

    bottomBar->addStretch();

    m_closeToTrayCheck = new QCheckBox(tr("Keep running in system tray"), this);
    m_closeToTrayCheck->setChecked(true);
    bottomBar->addWidget(m_closeToTrayCheck);

    m_discardBtn = new QPushButton(tr("Discard"), this);
    m_discardBtn->setEnabled(false);
    connect(m_discardBtn, &QPushButton::clicked, this, &MainWindow::onDiscardClicked);
    bottomBar->addWidget(m_discardBtn);

    m_saveBtn = new QPushButton(tr("Save && Apply"), this);
    m_saveBtn->setEnabled(false);
    m_saveBtn->setStyleSheet("QPushButton { font-weight: bold; padding: 6px 16px; }");
    connect(m_saveBtn, &QPushButton::clicked, this, &MainWindow::onSaveClicked);
    bottomBar->addWidget(m_saveBtn);

    mainLayout->addLayout(bottomBar);
}

void MainWindow::onTelemetryUpdated(const TelemetryData &data)
{
    m_currentServiceActive = data.serviceActive;
    m_currentMaxRpm = (data.maxRpm > 0) ? data.maxRpm : 4000;

    m_trayIcon->updateTelemetry(data);
    m_curveEditor->setCurrentTemperature(data.tempCelsius);

    // Update UI telemetry widgets
    if (data.serviceActive) {
        m_stateBadge->setText(tr("Active"));
        m_stateBadge->setStyleSheet("background-color: #27ae60; color: white; padding: 3px 10px; "
                                    "border-radius: 4px; font-weight: bold; font-size: 12px;");
        m_serviceToggleBtn->setText(tr("Switch to EC Mode"));
    } else if (data.serviceFailed) {
        m_stateBadge->setText(tr("Error (Daemon Crashed)"));
        m_stateBadge->setStyleSheet("background-color: #da4453; color: white; padding: 3px 10px; "
                                    "border-radius: 4px; font-weight: bold; font-size: 12px;");
        m_serviceToggleBtn->setText(tr("Restart Service"));
        if (!m_isModified) {
            m_statusMessage->setText(tr("Daemon crashed. Run 'sudo pacman -U lenovo-fanboost-0.3.0*' to update."));
            m_statusMessage->setStyleSheet("font-size: 11px; color: #da4453;");
        }
    } else {
        m_stateBadge->setText(tr("Inactive (EC Mode)"));
        m_stateBadge->setStyleSheet("background-color: #7f8c8d; color: white; padding: 3px 10px; "
                                    "border-radius: 4px; font-weight: bold; font-size: 12px;");
        m_serviceToggleBtn->setText(tr("Activate Fan Boost"));
    }

    if (data.tempCelsius > 0.0) {
        m_tempLabel->setText(QString("%1 °C").arg(data.tempCelsius, 0, 'f', 1));
        if (data.tempCelsius >= 80.0) {
            m_tempLabel->setStyleSheet("font-size: 18px; font-weight: bold; color: #da4453;");
        } else if (data.tempCelsius >= 65.0) {
            m_tempLabel->setStyleSheet("font-size: 18px; font-weight: bold; color: #f67400;");
        } else {
            m_tempLabel->setStyleSheet("font-size: 18px; font-weight: bold; color: #27ae60;");
        }
    } else {
        m_tempLabel->setText(tr("No sensor"));
        m_tempLabel->setStyleSheet("font-size: 18px; font-weight: bold; color: gray;");
    }

    if (data.targetRpm > 0 && m_currentMaxRpm > 0) {
        double pct = (static_cast<double>(data.targetRpm) / m_currentMaxRpm) * 100.0;
        m_targetLabel->setText(QString("%1 RPM (%2%)").arg(data.targetRpm).arg(pct, 0, 'f', 0));
    } else {
        m_targetLabel->setText(tr("0 RPM (EC Auto)"));
    }

    m_maxRpmLabel->setText(QString(tr("Max: %1 RPM")).arg(m_currentMaxRpm));
    m_ecSpeedLabel->setText(QString(tr("EC Speed: %1")).arg(data.ecRaw > 0 ? QString::number(data.ecRaw) : tr("--")));
}

void MainWindow::onConfigLoaded()
{
    updateProfileListUi();
    setModified(false);
}

void MainWindow::updateProfileListUi()
{
    QSignalBlocker blocker(m_profileCombo);
    m_profileCombo->clear();

    auto *model = new QStandardItemModel(this);

    // Section 1: Default Profiles
    auto *defaultHeader = new QStandardItem(tr("── Default Profiles ──"));
    defaultHeader->setEnabled(false);
    QFont hFont = defaultHeader->font();
    hFont.setBold(true);
    defaultHeader->setFont(hFont);
    defaultHeader->setForeground(palette().color(QPalette::PlaceholderText));
    model->appendRow(defaultHeader);

    const QStringList defaults = {"Quiet", "Balanced", "Aggressive"};
    for (const QString &name : defaults) {
        if (m_configMgr.profileNames().contains(name)) {
            auto *item = new QStandardItem(name);
            item->setData(name, Qt::UserRole);
            model->appendRow(item);
        }
    }

    // Section 2: Custom Profiles
    auto *customHeader = new QStandardItem(tr("── Custom Profiles ──"));
    customHeader->setEnabled(false);
    customHeader->setFont(hFont);
    customHeader->setForeground(palette().color(QPalette::PlaceholderText));
    model->appendRow(customHeader);

    QStringList customList;
    for (const QString &name : m_configMgr.profileNames()) {
        if (!m_configMgr.isDefaultProfile(name)) {
            customList.append(name);
        }
    }

    if (customList.isEmpty()) {
        auto *emptyItem = new QStandardItem(tr("  (No custom profiles)"));
        emptyItem->setEnabled(false);
        model->appendRow(emptyItem);
    } else {
        for (const QString &name : customList) {
            auto *item = new QStandardItem(name);
            item->setData(name, Qt::UserRole);
            model->appendRow(item);
        }
    }

    m_profileCombo->setModel(model);

    QString activeName = m_configMgr.activeProfile();
    for (int i = 0; i < model->rowCount(); ++i) {
        if (model->item(i)->data(Qt::UserRole).toString() == activeName) {
            m_profileCombo->setCurrentIndex(i);
            break;
        }
    }

    bool isCustom = !m_configMgr.isDefaultProfile(activeName);
    m_curveEditor->setPoints(m_configMgr.profile(activeName).points);
    m_renameProfileBtn->setEnabled(isCustom);
    m_deleteProfileBtn->setEnabled(isCustom);
    m_saveProfileBtn->setEnabled(isCustom);
    if (!isCustom) {
        m_saveProfileBtn->setToolTip(tr("Stock profiles ('Quiet', 'Balanced', 'Aggressive') are read-only presets. Drag points to create a custom curve."));
    } else {
        m_saveProfileBtn->setToolTip(tr("Save curve to profile"));
    }
}

void MainWindow::onProfileSelected(int index)
{
    if (index < 0) return;
    auto *model = qobject_cast<QStandardItemModel*>(m_profileCombo->model());
    if (!model) return;

    QStandardItem *item = model->item(index);
    if (!item || !item->isEnabled()) return;

    QString name = item->data(Qt::UserRole).toString();
    if (name.isEmpty()) return;

    m_configMgr.setActiveProfile(name);
    m_curveEditor->setPoints(m_configMgr.profile(name).points);

    bool isCustom = !m_configMgr.isDefaultProfile(name);
    m_renameProfileBtn->setEnabled(isCustom);
    m_deleteProfileBtn->setEnabled(isCustom);
    m_saveProfileBtn->setEnabled(isCustom);
    if (!isCustom) {
        m_saveProfileBtn->setToolTip(tr("Stock profiles ('Quiet', 'Balanced', 'Aggressive') are read-only presets. Drag points to create a custom curve."));
    } else {
        m_saveProfileBtn->setToolTip(tr("Save curve to profile"));
    }

    setModified(true);
}

void MainWindow::onCurveModified(const CurvePoints &points)
{
    QString current = m_configMgr.activeProfile();

    // If modifying a stock profile (Quiet, Balanced, Aggressive),
    // immediately switch to "Custom" so stock defaults stay pristine!
    if (m_configMgr.isDefaultProfile(current)) {
        QString customName = tr("Custom");
        m_configMgr.addOrUpdateProfile(customName, points);
        m_configMgr.setActiveProfile(customName);
        updateProfileListUi();
    } else {
        m_configMgr.addOrUpdateProfile(current, points);
    }

    // Live preview target RPM in KPI
    double curPct = m_curveEditor->interpolatePercentAt(m_curveEditor->currentTemperature());
    int curTarget = (curPct > 0) ? static_cast<int>(std::round((curPct / 100.0) * m_currentMaxRpm / 50.0)) * 50 : 0;
    if (m_currentServiceActive) {
        m_targetLabel->setText(QString("%1 RPM (%2%)").arg(curTarget).arg(curPct, 0, 'f', 0));
    }

    m_saveProfileBtn->setEnabled(true);
    setModified(true);
}

void MainWindow::onSaveProfileClicked()
{
    QString current = m_configMgr.activeProfile();
    if (m_configMgr.isDefaultProfile(current)) {
        QMessageBox::information(this, tr("Stock Presets Read-Only"),
                                 tr("Stock profiles ('Quiet', 'Balanced', 'Aggressive') cannot be overwritten. Edit the curve or use 'New...' to create a custom profile."));
        return;
    }

    bool ok = false;
    QString name = QInputDialog::getText(this, tr("Save Profile"),
                                         tr("Profile name:"), QLineEdit::Normal,
                                         current, &ok);
    name = name.trimmed();
    if (!ok || name.isEmpty()) return;

    if (m_configMgr.isDefaultProfile(name)) {
        QMessageBox::warning(this, tr("Protected Name"),
                             tr("Default profiles ('Quiet', 'Balanced', 'Aggressive') are reserved factory presets. Choose a custom name."));
        return;
    }

    CurvePoints currentPoints = m_curveEditor->points();
    if (name != current && !m_configMgr.isDefaultProfile(current)) {
        m_configMgr.removeProfile(current);
    }
    m_configMgr.addOrUpdateProfile(name, currentPoints);
    m_configMgr.setActiveProfile(name);
    updateProfileListUi();
    setModified(true);
    m_statusMessage->setText(QString(tr("Profile '%1' saved locally. Click 'Save & Apply' to activate it.")).arg(name));
}

void MainWindow::onRenameProfileClicked()
{
    QString current = m_configMgr.activeProfile();
    if (m_configMgr.isDefaultProfile(current)) {
        QMessageBox::information(this, tr("Protected Preset"),
                                 tr("Stock presets ('Quiet', 'Balanced', 'Aggressive') cannot be renamed."));
        return;
    }

    bool ok = false;
    QString newName = QInputDialog::getText(this, tr("Rename Profile"),
                                            tr("New profile name:"), QLineEdit::Normal,
                                            current, &ok);
    newName = newName.trimmed();
    if (!ok || newName.isEmpty() || newName == current) return;

    if (m_configMgr.isDefaultProfile(newName)) {
        QMessageBox::warning(this, tr("Reserved Name"),
                             tr("Names 'Quiet', 'Balanced', and 'Aggressive' are reserved for stock presets."));
        return;
    }

    if (m_configMgr.profileNames().contains(newName)) {
        QMessageBox::warning(this, tr("Name Exists"),
                             tr("A profile with this name already exists."));
        return;
    }

    CurvePoints pts = m_configMgr.profile(current).points;
    m_configMgr.removeProfile(current);
    m_configMgr.addOrUpdateProfile(newName, pts);
    m_configMgr.setActiveProfile(newName);
    updateProfileListUi();
    setModified(true);
}

void MainWindow::onNewProfileClicked()
{
    bool ok = false;
    QString name = QInputDialog::getText(this, tr("Create Profile"),
                                         tr("New profile name:"), QLineEdit::Normal,
                                         tr("Custom"), &ok);
    name = name.trimmed();
    if (!ok || name.isEmpty()) return;

    if (m_configMgr.profileNames().contains(name)) {
        QMessageBox::warning(this, tr("Profile Exists"), tr("A profile with this name already exists."));
        return;
    }

    CurvePoints currentPoints = m_curveEditor->points();
    m_configMgr.addOrUpdateProfile(name, currentPoints);
    m_configMgr.setActiveProfile(name);
    updateProfileListUi();
    setModified(true);
}

void MainWindow::onDeleteProfileClicked()
{
    QString current = m_profileCombo->currentText();
    if (m_configMgr.isDefaultProfile(current)) {
        return;
    }

    auto btn = QMessageBox::question(this, tr("Delete Profile"),
                                     QString(tr("Delete custom profile '%1'?")).arg(current),
                                     QMessageBox::Yes | QMessageBox::No);
    if (btn == QMessageBox::Yes) {
        m_configMgr.removeProfile(current);
        updateProfileListUi();
        setModified(true);
    }
}

void MainWindow::setModified(bool modified)
{
    m_isModified = modified;
    m_saveBtn->setEnabled(modified);
    m_discardBtn->setEnabled(modified);
    if (modified) {
        m_statusMessage->setText(tr("Unsaved modifications — click Save & Apply to write changes."));
        m_statusMessage->setStyleSheet("font-size: 11px; color: #f67400;");
    } else {
        m_statusMessage->setText(tr("All changes saved."));
        m_statusMessage->setStyleSheet("font-size: 11px; color: gray;");
    }
}

void MainWindow::onSaveClicked()
{
    QString err;
    if (!m_configMgr.save("/etc/lenovo-fanboost.toml", &err)) {
        QMessageBox::critical(this, tr("Save Error"), QString(tr("Failed to save configuration:\n%1")).arg(err));
        return;
    }
    setModified(false);
    m_statusMessage->setText(tr("Configuration saved and applied."));
    m_statusMessage->setStyleSheet("font-size: 11px; color: #27ae60;");
}

void MainWindow::onDiscardClicked()
{
    m_configMgr.load();
    setModified(false);
    m_statusMessage->setText(tr("Changes discarded."));
    m_statusMessage->setStyleSheet("font-size: 11px; color: gray;");
}

void MainWindow::onToggleServiceClicked()
{
    bool newState = !m_currentServiceActive;
    QString err;
    if (!TelemetryMonitor::setServiceActive(newState, &err)) {
        QMessageBox::warning(this, tr("Service Control Error"),
                             QString(tr("Failed to change service state:\n%1")).arg(err));
    }
    m_telemetry.refreshNow();
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (m_closeToTrayCheck->isChecked() && m_trayIcon->isVisible()) {
        hide();
        event->ignore();
    } else {
        event->accept();
    }
}
