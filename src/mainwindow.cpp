#include "mainwindow.h"
#include "autostart.h"
#include "profile_io.h"
#include "session_history.h"
#include <QAccessible>
#include <QPainter>
#include <QPainterPath>
#include <QStyle>

#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QDialog>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QFrame>
#include <QGridLayout>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollArea>
#include <QShortcut>
#include <QSignalBlocker>
#include <QStackedWidget>
#include <QStyleHints>
#include <QTextEdit>
#include <QTimer>
#include <QVBoxLayout>

namespace
{
QIcon profileIcon(const QString &name, const QColor &color)
{
    QPixmap image(64, 64);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.scale(2, 2);
    painter.setPen(QPen(color, 1.7, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.setBrush(Qt::NoBrush);
    if (name == "Quiet") {
        QPainterPath leaf;
        leaf.moveTo(7, 25);
        leaf.cubicTo(3, 11, 12, 6, 27, 5);
        leaf.cubicTo(28, 20, 18, 29, 7, 25);
        painter.drawPath(leaf);
        painter.drawLine(QPointF(6, 28), QPointF(23, 10));
    } else if (name == "Balanced") {
        painter.drawLine(16, 5, 16, 27);
        painter.drawLine(6, 11, 26, 11);
        painter.drawLine(11, 27, 21, 27);
        painter.drawPolyline(QPolygonF{QPointF(3, 21), QPointF(7, 12), QPointF(11, 21)});
        painter.drawPolyline(QPolygonF{QPointF(21, 21), QPointF(25, 12), QPointF(29, 21)});
        painter.drawArc(QRectF(3, 17, 8, 8), 180 * 16, 180 * 16);
        painter.drawArc(QRectF(21, 17, 8, 8), 180 * 16, 180 * 16);
    } else if (name == "Aggressive") {
        painter.drawPolygon(QPolygonF{QPointF(19, 3), QPointF(7, 18), QPointF(15, 18), QPointF(12, 29),
                                      QPointF(26, 13), QPointF(18, 13)});
    } else {
        painter.drawPolyline(QPolygonF{QPointF(4, 25), QPointF(12, 20), QPointF(19, 11), QPointF(28, 6)});
        for (const QPointF point : {QPointF(4, 25), QPointF(12, 20), QPointF(19, 11), QPointF(28, 6)}) {
            painter.setBrush(color);
            painter.drawEllipse(point, 1.8, 1.8);
        }
    }
    return QIcon(image);
}

} // namespace

MainWindow::MainWindow(QWidget *parent, bool previewMode)
    : QMainWindow(parent), m_previewMode(previewMode),
      m_settings(QStringLiteral("Lenovo"), QStringLiteral("FanBoost"))
{
    setWindowTitle(tr("Lenovo Fan Boost"));
    setWindowIcon(qApp->windowIcon());
    resize(1180, 780);
    setMinimumSize(600, 440);
    m_theme = m_settings.value("appearance/theme", "system").toString();
    m_savedCloseToTray = m_settings.value("window/closeToTray", true).toBool();
    m_trayIcon = new TrayIcon(&m_configMgr, this);
    setupUi();
    setupSessionTools();
    applyTheme(m_theme);
    updateStateLayout();
    if (!m_previewMode)
        restoreGeometry(m_settings.value("window/geometry").toByteArray());

    connect(&m_telemetry, &TelemetryMonitor::telemetryUpdated, this, &MainWindow::onTelemetryUpdated);
    connect(&m_telemetry, &TelemetryMonitor::busyChanged, this, [this](bool busy) {
        if (busy)
            showStatus(tr("System operation pending…"));
        updateBusyState();
    });
    connect(&m_telemetry, &TelemetryMonitor::operationFinished, this,
            [this](bool ok, const QString &message) {
                if (m_modeOperationPending) {
                    m_modeOperationPending = false;
                    if (!ok) {
                        m_modePending = false;
                        m_pendingMode = -1;
                        syncModeSelector();
                        updateBusyState();
                        showStatus(tr("Mode change failed: %1").arg(message));
                    } else if (m_modePending) {
                        showStatus(tr("Mode request accepted; confirming service state…"));
                        QTimer::singleShot(8000, this, [this] {
                            if (!m_modePending)
                                return;
                            m_modePending = false;
                            m_pendingMode = -1;
                            syncModeSelector();
                            updateBusyState();
                            showStatus(tr("Mode change could not be confirmed. Refresh service status."));
                            m_telemetry.refreshNow();
                        });
                    }
                } else {
                    showStatus(ok ? tr("System operation completed; refreshing status.")
                                  : tr("System operation failed: %1").arg(message));
                    m_telemetry.refreshNow();
                }
            });
    connect(&m_configMgr, &ConfigManager::configLoaded, this, &MainWindow::onConfigLoaded);
    connect(&m_configMgr, &ConfigManager::busyChanged, this, [this](bool busy) {
        if (busy)
            showStatus(tr("Saving configuration and checking service…"));
        updateBusyState();
    });
    connect(&m_configMgr, &ConfigManager::saveFinished, this,
            [this](bool ok, const QString &message, bool restarted) {
                updateBusyState();
                if (ok) {
                    m_implicitCopySource.clear();
                    m_implicitCopyName.clear();
                    m_configuredProfile = m_savingProfile;
                    m_configuredValue->setText(m_configuredProfile);
                    m_trayIcon->setConfiguredProfile(m_configuredProfile);
                    m_savedFingerprint = configFingerprint();
                    setModified(false);
                    showStatus(restarted ? tr("Configuration saved; service restart confirmed.")
                                         : tr("Configuration saved. Service state is not confirmed."));
                    if (m_startServiceAfterSave) {
                        m_startServiceAfterSave = false;
                        if (!m_serviceKnown || !m_currentServiceActive)
                            requestMode(1, true);
                    }
                } else {
                    m_startServiceAfterSave = false;
                    showStatus(tr("Save failed: %1").arg(message));
                    if (m_closingAfterApply) {
                        showNormal();
                        raise();
                        activateWindow();
                    }
                }
                m_applyButton->setEnabled(m_isModified);
                m_discardButton->setEnabled(m_isModified);
                if (m_closingAfterApply && ok) {
                    m_closingAfterApply = false;
                    qApp->quit();
                } else if (m_closingAfterApply && !ok)
                    m_closingAfterApply = false;
                if (ok && !m_pendingProfile.isEmpty()) {
                    const QString next = m_pendingProfile;
                    m_pendingProfile.clear();
                    selectProfile(next);
                } else if (!ok) {
                    m_pendingProfile.clear();
                }
            });
    connect(m_trayIcon, &TrayIcon::openWindowRequested, this, [this] {
        showNormal();
        activateWindow();
        raise();
    });
    connect(m_trayIcon, &TrayIcon::profileChangeRequested, this, [this](const QString &name) {
        if (m_operationBusy) {
            showStatus(tr("Wait for the pending operation before changing profiles."));
            return;
        }
        selectProfile(name);
        m_trayIcon->updateProfileList();
        showNormal();
        raise();
        activateWindow();
    });
    connect(m_trayIcon, &TrayIcon::serviceToggleRequested, this, [this](bool active) {
        if (m_previewMode)
            return;
        requestMode(active ? 1 : 0);
    });
    connect(m_trayIcon, &TrayIcon::quitRequested, this, [this] {
        if (m_operationBusy) {
            showStatus(tr("Wait for the pending operation to finish before quitting."));
            showNormal();
            raise();
            activateWindow();
            return;
        }
        if (m_isModified) {
            QMessageBox box(QMessageBox::Question, tr("Unsaved changes"),
                            tr("Save the current draft before quitting?"), QMessageBox::NoButton, this);
            auto *apply = box.addButton(tr("Save draft"), QMessageBox::AcceptRole);
            auto *discard = box.addButton(tr("Discard changes"), QMessageBox::DestructiveRole);
            auto *cancel = box.addButton(QMessageBox::Cancel);
            box.exec();
            if (box.clickedButton() == cancel)
                return;
            if (box.clickedButton() == apply) {
                m_pendingProfile.clear();
                m_closingAfterApply = true;
                applyChanges();
                return;
            }
            Q_UNUSED(discard);
            onDiscardClicked();
        }
        qApp->quit();
    });

    if (m_previewMode) {
        m_startServiceAfterSave = false;
        m_configMgr.addOrUpdateProfile("Quiet", ConfigManager::defaultQuietPoints());
        m_configMgr.addOrUpdateProfile("Balanced",
                                       {{40, 0}, {50, 0}, {60, 45}, {70, 65}, {80, 85}, {90, 100}});
        m_configMgr.addOrUpdateProfile("Aggressive", ConfigManager::defaultAggressivePoints());
        m_configMgr.addOrUpdateProfile("Custom",
                                       {{40, 0}, {50, 10}, {60, 40}, {70, 65}, {80, 85}, {90, 100}});
        m_configMgr.setActiveProfile("Balanced");
        m_previewProfile = "Balanced";
        m_savedFingerprint = configFingerprint();
        m_configuredProfile = "Balanced";
        updateProfileListUi();
        m_serviceValue->setText(tr("Service running"));
        m_tempValue->setText(tr("68 °C"));
        m_targetValue->setText(tr("2,400 RPM"));
        m_currentServiceActive = true;
        m_serviceKnown = true;
        m_confirmedMode = 1;
        syncModeSelector();
        m_configuredValue->setText("Balanced");
        m_curveEditor->setTelemetryAvailable(true);
        m_curveEditor->setCurrentTemperature(68);
        showStatus(tr("Preview sample data. System operations are disabled."));
        m_modeSelector->setEnabled(false);
        m_refreshStatusButton->setEnabled(false);
        m_applyButton->setEnabled(false);
        m_trayIcon->setVisible(false);
    } else {
        const bool loaded = m_configMgr.load();
        if (!loaded) {
            m_configuredProfile.clear();
            m_configuredValue->setText(tr("Unknown"));
            showStatus(tr("Configuration could not be read. The saved profile is unknown."));
        } else {
            m_configuredProfile = m_configMgr.activeProfile();
            m_previewProfile = m_configuredProfile;
            m_configuredValue->setText(m_configuredProfile);
            m_trayIcon->setConfiguredProfile(m_configuredProfile);
            m_savedFingerprint = configFingerprint();
            updateProfileListUi();
            setModified(false);
        }
        m_telemetry.start(1000);
        if (QSystemTrayIcon::isSystemTrayAvailable())
            m_trayIcon->show();
        else
            m_closeToTray->setEnabled(false);
    }
}

void MainWindow::setupUi()
{
    auto *root = new QWidget(this);
    auto *outer = new QVBoxLayout(root);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    m_serviceValue = new QLabel(tr("Checking service status"), root);
    m_tempValue = new QLabel(tr("Unavailable"), root);
    m_targetValue = new QLabel(tr("Unavailable"), root);
    m_configuredValue = new QLabel(tr("Unknown"), root);
    m_tempValue->setObjectName("metricValue");
    m_targetValue->setObjectName("metricValue");
    m_modeSelector = new QComboBox(root);
    m_modeSelector->setObjectName("serviceMode");
    m_modeSelector->addItem(tr("Firmware automatic"), 0);
    m_modeSelector->addItem(tr("Fan curve"), 1);
    m_modeSelector->setMinimumWidth(190);
    m_modeSelector->setAccessibleName(tr("Fan control mode"));
    m_modeSelector->setAccessibleDescription(tr("Changing this selection immediately requests a service mode "
                                                "change. It does not apply pending profile edits."));
    connect(m_modeSelector, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &MainWindow::onModeSelected);
    m_modeSelector->setPlaceholderText(tr("Checking service status"));
    syncModeSelector();
    m_modeSelector->setEnabled(false);
    m_modeControls = new QWidget(root);
    auto *modeLayout = new QHBoxLayout(m_modeControls);
    modeLayout->setContentsMargins(0, 0, 0, 0);
    modeLayout->setSpacing(6);
    modeLayout->addWidget(m_modeSelector, 1);
    m_refreshStatusButton = new QPushButton(tr("Refresh"), m_modeControls);
    m_refreshStatusButton->setObjectName("refreshServiceStatus");
    m_refreshStatusButton->setAccessibleName(tr("Refresh service status"));
    connect(m_refreshStatusButton, &QPushButton::clicked, this, [this] {
        showStatus(tr("Refreshing service status…"));
        m_telemetry.refreshNow();
    });
    modeLayout->addWidget(m_refreshStatusButton);

    auto *body = new QWidget(root);
    auto *bodyLayout = new QHBoxLayout(body);
    bodyLayout->setContentsMargins(0, 0, 0, 0);
    bodyLayout->setSpacing(0);
    auto *sidebar = new QWidget(body);
    sidebar->setObjectName("sidebar");
    sidebar->setMinimumWidth(220);
    sidebar->setMaximumWidth(270);
    auto *side = new QVBoxLayout(sidebar);
    side->setContentsMargins(12, 18, 12, 14);
    auto *controlHeading = new QLabel(tr("Fan control"), sidebar);
    controlHeading->setObjectName("sectionHeading");
    side->addWidget(controlHeading);
    m_modeSelector->setMinimumWidth(0);
    m_modeSelector->setMinimumContentsLength(10);
    m_modeSelector->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    m_refreshStatusButton->setText(QString());
    m_refreshStatusButton->setIcon(style()->standardIcon(QStyle::SP_BrowserReload));
    m_refreshStatusButton->setToolTip(tr("Refresh service status"));
    side->addWidget(m_modeControls);
    m_serviceValue->setWordWrap(true);
    side->addWidget(m_serviceValue);
    auto *configuredCaption = new QLabel(tr("Configured profile"), sidebar);
    configuredCaption->setObjectName("secondaryText");
    side->addWidget(configuredCaption);
    side->addWidget(m_configuredValue);
    side->addSpacing(16);
    auto *profilesHeading = new QLabel(tr("Profiles"), sidebar);
    profilesHeading->setObjectName("sectionHeading");
    side->addWidget(profilesHeading);
    m_profiles = new QListWidget(sidebar);
    m_profiles->setObjectName("profiles");
    m_profiles->setAccessibleName(tr("Fan profiles"));
    m_profiles->setSelectionMode(QAbstractItemView::SingleSelection);
    m_profiles->setIconSize(QSize(26, 26));
    QFont profileFont = qApp->font();
    profileFont.setPointSizeF(profileFont.pointSizeF() * 1.12);
    m_profiles->setFont(profileFont);
    m_profiles->setMinimumHeight(profileFont.pointSizeF() * 18);
    m_profiles->setAccessibleDescription(tr("Select a profile to view or edit it. Use Save and use to apply it. Editing a built-in profile creates a custom draft."));
    connect(m_profiles, &QListWidget::currentRowChanged, this, [this](int row) {
        if (row>=0 && !m_profiles->item(row)->data(Qt::UserRole).toString().isEmpty()) showPage(0);
    });
    auto activateItem = [this](QListWidgetItem *item) {
        const QString name = item->data(Qt::UserRole).toString();
        if (name.isEmpty()) return;
        showPage(0);
        QTimer::singleShot(0,this,[this,name] { selectProfile(name); });
    };
    connect(m_profiles,&QListWidget::itemClicked,this,activateItem);
    connect(m_profiles,&QListWidget::itemActivated,this,activateItem);
    side->addWidget(m_profiles, 1);
    m_createProfileButton = new QPushButton(tr("Create profile"), sidebar);
    m_createProfileButton->setObjectName("createProfile");
    connect(m_createProfileButton, &QPushButton::clicked, this, &MainWindow::createProfile);
    side->addWidget(m_createProfileButton);
    m_importProfileButton = new QPushButton(tr("Import profile…"), sidebar);
    m_importProfileButton->setObjectName("importProfile");
    connect(m_importProfileButton, &QPushButton::clicked, this, [this] {
        const QString path = QFileDialog::getOpenFileName(this, tr("Import profile"), {},
                                                          tr("Fan profile (*.json);;All files (*)"));
        if (path.isEmpty())
            return;
        FanProfile imported;
        QString error;
        if (!ProfileIO::read(path, &imported, &error)) {
            QMessageBox::warning(this, tr("Import failed"), error);
            return;
        }
        if (m_configMgr.isDefaultProfile(imported.name)) {
            QMessageBox::warning(this, tr("Reserved profile name"),
                                 tr("Imported profiles cannot replace built-in profiles."));
            return;
        }
        const bool exists = m_configMgr.profileNames().contains(imported.name);
        if (exists && QMessageBox::question(
                          this, tr("Replace profile"),
                          tr("Replace custom profile '%1' in the pending configuration?").arg(imported.name),
                          QMessageBox::Yes | QMessageBox::Cancel) != QMessageBox::Yes)
            return;
        m_configMgr.addOrUpdateProfile(imported.name, imported.points);
        m_configMgr.setActiveProfile(imported.name);
        m_previewProfile = imported.name;
        updateProfileListUi();
        setModified(true);
    });
    side->addWidget(m_importProfileButton);
    m_profileActions = new QWidget(sidebar);
    auto *profileActionsLayout = new QVBoxLayout(m_profileActions);
    profileActionsLayout->setContentsMargins(0, 0, 0, 0);
    m_duplicateProfileButton = new QPushButton(tr("Duplicate"), m_profileActions);
    m_duplicateProfileButton->setObjectName("duplicateProfile");
    m_renameProfileButton = new QPushButton(tr("Rename"), m_profileActions);
    m_renameProfileButton->setObjectName("renameProfile");
    m_deleteProfileButton = new QPushButton(tr("Delete"), m_profileActions);
    m_deleteProfileButton->setObjectName("deleteProfile");
    m_exportProfileButton = new QPushButton(tr("Export…"), m_profileActions);
    m_exportProfileButton->setObjectName("exportProfile");
    connect(m_duplicateProfileButton, &QPushButton::clicked, this, &MainWindow::duplicateProfile);
    connect(m_renameProfileButton, &QPushButton::clicked, this, &MainWindow::renameProfile);
    connect(m_deleteProfileButton, &QPushButton::clicked, this, &MainWindow::deleteProfile);
    connect(m_exportProfileButton, &QPushButton::clicked, this, [this] {
        const QString name = m_previewProfile;
        if (name.isEmpty() || m_configMgr.isDefaultProfile(name))
            return;
        QString path = QFileDialog::getSaveFileName(this, tr("Export profile"), name + ".json",
                                                    tr("Fan profile (*.json)"));
        if (path.isEmpty())
            return;
        if (!path.endsWith(".json", Qt::CaseInsensitive))
            path += ".json";
        QString error;
        if (!ProfileIO::write(path, m_configMgr.profile(name), &error))
            QMessageBox::warning(this, tr("Export failed"), error);
        else
            showStatus(tr("Profile exported."));
    });
    profileActionsLayout->addWidget(m_duplicateProfileButton);
    profileActionsLayout->addWidget(m_renameProfileButton);
    profileActionsLayout->addWidget(m_deleteProfileButton);
    profileActionsLayout->addWidget(m_exportProfileButton);
    side->addWidget(m_profileActions);
    auto *settingsButton = new QPushButton(tr("Settings"), sidebar);
    auto *diagnosticsButton = new QPushButton(tr("Diagnostics"), sidebar);
    connect(settingsButton, &QPushButton::clicked, this, [this] { showPage(1); });
    connect(diagnosticsButton, &QPushButton::clicked, this, [this] { showPage(2); });
    side->addWidget(settingsButton);
    side->addWidget(diagnosticsButton);
    auto *sideScroll = new QScrollArea(body);
    sideScroll->setFrameShape(QFrame::NoFrame);
    sideScroll->setWidgetResizable(true);
    sideScroll->setMinimumWidth(238);
    sideScroll->setMaximumWidth(288);
    sideScroll->setWidget(sidebar);
    bodyLayout->addWidget(sideScroll);

    m_pages = new QStackedWidget(body);
    auto *workspace = new QWidget(m_pages);
    auto *work = new QVBoxLayout(workspace);
    work->setContentsMargins(22, 20, 22, 14);
    m_headingLayout = new QGridLayout;
    m_headingNames = new QWidget(workspace);
    auto *names = new QVBoxLayout(m_headingNames);
    names->setContentsMargins(0, 0, 0, 0);
    m_editorTitle = new QLabel(tr("Balanced"), workspace);
    m_editorTitle->setObjectName("pageTitle");
    m_editorTitle->setWordWrap(true);
    m_editorSubTitle = new QLabel(tr("Selected profile"), workspace);
    m_editorSubTitle->setObjectName("secondaryText");
    names->addWidget(m_editorTitle);
    names->addWidget(m_editorSubTitle);
    m_headingActions = new QWidget(workspace);
    auto *historyActions = new QHBoxLayout(m_headingActions);
    historyActions->setContentsMargins(0, 0, 0, 0);
    auto *undo = new QPushButton(tr("Undo"), workspace);
    auto *redo = new QPushButton(tr("Redo"), workspace);
    undo->setEnabled(false);
    redo->setEnabled(false);
    historyActions->addWidget(undo);
    historyActions->addWidget(redo);
    work->addLayout(m_headingLayout);
    m_readingsLayout = new QGridLayout;
    m_readingsLayout->setContentsMargins(0, 8, 0, 12);
    m_readingsLayout->setHorizontalSpacing(10);
    m_cpuCaption = new QLabel(tr("CPU temperature"), workspace);
    m_requestCaption = new QLabel(tr("Fan request"), workspace);
    m_cpuCaption->setObjectName("secondaryText");
    m_requestCaption->setObjectName("secondaryText");
    m_tempValue->setAccessibleName(tr("CPU temperature"));
    m_targetValue->setAccessibleName(tr("Requested fan speed"));
    work->addLayout(m_readingsLayout);
    m_curveEditor = new CurveEditor(workspace);
    m_curveEditor->setObjectName("curveEditor");
    connect(undo, &QPushButton::clicked, m_curveEditor, &CurveEditor::undo);
    connect(redo, &QPushButton::clicked, m_curveEditor, &CurveEditor::redo);
    connect(m_curveEditor, &CurveEditor::undoAvailabilityChanged, undo,
            [undo](bool enabled, bool) { undo->setEnabled(enabled); });
    connect(m_curveEditor, &CurveEditor::undoAvailabilityChanged, this,
            [redo](bool, bool enabled) { redo->setEnabled(enabled); });
    connect(m_curveEditor, &CurveEditor::curveChanged, this, &MainWindow::onCurveModified);
    work->addWidget(m_curveEditor, 1);
    m_pages->addWidget(workspace);

    auto *settingsPage = new QWidget(m_pages);
    settingsPage->setObjectName("settingsPage");
    auto *settings = new QVBoxLayout(settingsPage);
    settings->setContentsMargins(24, 24, 24, 24);
    auto *settingsTitle = new QLabel(tr("Settings"), settingsPage);
    settingsTitle->setObjectName("pageTitle");
    settings->addWidget(settingsTitle);
    m_closeToTray = new QCheckBox(tr("Keep running in the system tray when the window closes"), settingsPage);
    m_closeToTray->setChecked(m_savedCloseToTray);
    m_closeToTray->setAccessibleDescription(
        tr("The fan control service continues independently when the window closes."));
    settings->addWidget(m_closeToTray);
    connect(m_closeToTray, &QCheckBox::toggled, this, [this](bool enabled) {
        m_savedCloseToTray = enabled;
        if (!m_previewMode)
            m_settings.setValue("window/closeToTray", enabled);
    });
    auto *themeLabel = new QLabel(tr("Appearance"), settingsPage);
    m_themeCombo = new QComboBox(settingsPage);
    m_themeCombo->setObjectName("appearanceTheme");
    m_themeCombo->addItem(tr("Follow system"), "system");
    m_themeCombo->addItem(tr("Light"), "light");
    m_themeCombo->addItem(tr("Dark"), "dark");
    m_themeCombo->setCurrentIndex(m_themeCombo->findData(m_theme));
    connect(m_themeCombo, &QComboBox::currentIndexChanged, this, [this](int i) {
        m_theme = m_themeCombo->itemData(i).toString();
        if (!m_previewMode)
            m_settings.setValue("appearance/theme", m_theme);
        applyTheme(m_theme);
    });
    settings->addWidget(themeLabel);
    settings->addWidget(m_themeCombo);
    auto *bootTitle = new QLabel(tr("Fan control at boot"), settingsPage);
    settings->addWidget(bootTitle);
    auto *bootActions = new QHBoxLayout;
    auto *enableBoot = new QPushButton(tr("Enable at boot"), settingsPage);
    auto *disableBoot = new QPushButton(tr("Disable at boot"), settingsPage);
    enableBoot->setObjectName("enableBoot");
    disableBoot->setObjectName("disableBoot");
    enableBoot->setEnabled(!m_previewMode);
    disableBoot->setEnabled(!m_previewMode);
    connect(enableBoot, &QPushButton::clicked, this, [this] { m_telemetry.requestStartAtBoot(true); });
    connect(disableBoot, &QPushButton::clicked, this, [this] { m_telemetry.requestStartAtBoot(false); });
    bootActions->addWidget(enableBoot);
    bootActions->addWidget(disableBoot);
    bootActions->addStretch();
    settings->addLayout(bootActions);
    m_bootStatus = new QLabel(tr("Boot startup status: unknown"), settingsPage);
    m_bootStatus->setObjectName("secondaryText");
    settings->addWidget(m_bootStatus);
    auto *bootNote = new QLabel(
        tr("Controls the system service at boot. This is separate from starting the application at login."),
        settingsPage);
    bootNote->setWordWrap(true);
    bootNote->setObjectName("secondaryText");
    settings->addWidget(bootNote);
    auto *closeNote = new QLabel(
        tr("Closing this window does not stop fan control. The service runs independently."), settingsPage);
    closeNote->setWordWrap(true);
    closeNote->setObjectName("secondaryText");
    settings->addWidget(closeNote);
    settings->addStretch();
    m_pages->addWidget(settingsPage);

    auto *diagnosticsPage = new QWidget(m_pages);
    diagnosticsPage->setObjectName("diagnosticsPage");
    auto *diagLayout = new QVBoxLayout(diagnosticsPage);
    diagLayout->setContentsMargins(24, 24, 24, 24);
    auto *diagTitle = new QLabel(tr("Diagnostics"), diagnosticsPage);
    diagTitle->setObjectName("pageTitle");
    diagLayout->addWidget(diagTitle);
    m_diagnosticsText = new QLabel(tr("Waiting for status information."), diagnosticsPage);
    m_diagnosticsText->setWordWrap(true);
    m_diagnosticsText->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_diagnosticsText->setAccessibleName(tr("Current system diagnostics"));
    diagLayout->addWidget(m_diagnosticsText);
    auto *copy = new QPushButton(tr("Copy diagnostics"), diagnosticsPage);
    connect(copy, &QPushButton::clicked, this, [this] {
        QGuiApplication::clipboard()->setText(m_diagnosticsText->text());
        showStatus(tr("Diagnostics copied to clipboard."));
    });
    diagLayout->addWidget(copy, 0, Qt::AlignLeft);
    diagLayout->addStretch();
    m_pages->addWidget(diagnosticsPage);
    auto *pageScroll = new QScrollArea(body);
    pageScroll->setFrameShape(QFrame::NoFrame);
    pageScroll->setWidgetResizable(true);
    pageScroll->setWidget(m_pages);
    bodyLayout->addWidget(pageScroll, 1);
    outer->addWidget(body, 1);

    auto *footer = new QWidget(root);
    footer->setObjectName("footer");
    auto *foot = new QHBoxLayout(footer);
    foot->setContentsMargins(20, 10, 18, 10);
    m_statusMessage = new QLabel(tr("Loading configuration…"), footer);
    m_statusMessage->setWordWrap(true);
    m_statusMessage->setObjectName("statusMessage");
    foot->addWidget(m_statusMessage, 1);
    m_discardButton = new QPushButton(tr("Discard changes"), footer);
    m_discardButton->setObjectName("discardChanges");
    m_applyButton = new QPushButton(tr("Save profile"), footer);
    m_applyButton->setObjectName("applyChanges");
    m_discardButton->setMinimumWidth(135);
    m_applyButton->setMinimumWidth(135);
    connect(m_discardButton, &QPushButton::clicked, this, &MainWindow::onDiscardClicked);
    connect(m_applyButton, &QPushButton::clicked, this, &MainWindow::onSaveClicked);
    foot->addWidget(m_discardButton);
    foot->addWidget(m_applyButton);
    outer->addWidget(footer);
    setCentralWidget(root);
    setTabOrder(m_profiles, m_curveEditor);
    m_discardButton->setEnabled(false);
    m_applyButton->setEnabled(false);
}

void MainWindow::applyTheme(const QString &theme)
{
    const bool dark =
        theme == "dark" || (theme == "system" && qApp->styleHints()->colorScheme() == Qt::ColorScheme::Dark);
    QPalette p = qApp->palette();
    if (dark) {
        p.setColor(QPalette::Window, QColor("#202124"));
        p.setColor(QPalette::Base, QColor("#292b2f"));
        p.setColor(QPalette::AlternateBase, QColor("#303238"));
        p.setColor(QPalette::Button, QColor("#34363b"));
        p.setColor(QPalette::Text, QColor("#f1f1f1"));
        p.setColor(QPalette::WindowText, QColor("#f1f1f1"));
        p.setColor(QPalette::ButtonText, QColor("#f1f1f1"));
        p.setColor(QPalette::Highlight, QColor("#51343e"));
        p.setColor(QPalette::HighlightedText, QColor("#ffe8ed"));
        p.setColor(QPalette::PlaceholderText, QColor("#bec2ca"));
    } else {
        p.setColor(QPalette::Window, QColor("#f4f5f6"));
        p.setColor(QPalette::Base, Qt::white);
        p.setColor(QPalette::AlternateBase, QColor("#f2f3f5"));
        p.setColor(QPalette::Button, QColor("#f7f7f8"));
        p.setColor(QPalette::Text, QColor("#202124"));
        p.setColor(QPalette::WindowText, QColor("#202124"));
        p.setColor(QPalette::ButtonText, QColor("#202124"));
        p.setColor(QPalette::Highlight, QColor("#f6e3e7"));
        p.setColor(QPalette::HighlightedText, QColor("#851126"));
        p.setColor(QPalette::PlaceholderText, QColor("#515660"));
    }
    p.setColor(QPalette::Link, dark ? QColor("#ff8c9b") : QColor("#c8102e"));
    qApp->setPalette(p);
    for (int row = 0; row < m_profiles->count(); ++row) {
        auto *item = m_profiles->item(row);
        if (item->data(Qt::UserRole).toString().isEmpty())
            item->setForeground(p.color(QPalette::PlaceholderText));
    }
    for (int row = 0; row < m_profiles->count(); ++row) {
        auto *item = m_profiles->item(row);
        const QString name = item->data(Qt::UserRole).toString();
        if (!name.isEmpty())
            item->setIcon(profileIcon(m_configMgr.isDefaultProfile(name) ? name : QStringLiteral("Custom"),
                                      p.color(QPalette::Text)));
    }
    QFont titleFont = qApp->font();
    titleFont.setPointSizeF(titleFont.pointSizeF() * 1.5);
    m_editorTitle->setFont(titleFont);
    const QString stroke = dark ? "#59616d" : "#b7bec8";
    setStyleSheet(
        QStringLiteral(
            "QWidget#workspace,QWidget#settingsPage,QWidget#diagnosticsPage { background: palette(base); }"
            "QWidget#stateBar { background: palette(window); border-bottom: 1px solid %1; }"
            "QWidget#footer { background: palette(window); border-top: 1px solid %1; }"
            "QWidget#sidebar { background: palette(alternate-base); border-right: 1px solid %1; }"
            "QLabel#metricCaption,QLabel#secondaryText { color: palette(placeholder-text); }"
            "QLabel#metricValue,QLabel#pageTitle,QLabel#sectionHeading { font-weight: 600; }"
            "QListWidget#profiles { border: none; background: transparent; }"
            "QListWidget::item { padding: 8px 10px; border-radius: 6px; min-height: 28px; }"
            "QListWidget::item:selected { background: palette(highlight); color: palette(highlighted-text); "
            "font-weight: 600; }"
            "QListWidget::item:hover { background: palette(base); }"
            "QListWidget::item:focus { border: 2px solid palette(link); }"
            "QPushButton { min-height: 26px; padding: 7px 12px; border: 1px solid %1; border-radius: 6px; "
            "background: palette(button); color: palette(button-text); }"
            "QPushButton:hover { background: palette(base); border-color: palette(link); }"
            "QPushButton:focus { border: 2px solid palette(link); padding: 6px 11px; }"
            "QPushButton:disabled { color: palette(placeholder-text); }"
            "QPushButton#applyChanges,QPushButton#createProfile { background: #b51225; color: white; "
            "border-color: #b51225; font-weight: 600; }"
            "QPushButton#applyChanges:hover,QPushButton#createProfile:hover { background: #941020; }"
            "QPushButton#applyChanges:focus,QPushButton#createProfile:focus { border: 2px solid "
            "palette(link); }"
            "QPushButton#applyChanges:disabled { background: palette(button); color: "
            "palette(placeholder-text); border-color: %1; }"
            "QComboBox,QLineEdit { min-height: 26px; padding: 6px 10px; border: 1px solid %1; border-radius: "
            "6px; background: palette(base); color: palette(text); }"
            "QComboBox:focus,QLineEdit:focus { border: 2px solid palette(link); padding: 5px 9px; }"
            "QTableView { border: 1px solid %1; border-radius: 6px; gridline-color: %1; }"
            "QHeaderView::section { padding: 8px; background: palette(alternate-base); color: palette(text); "
            "border: none; border-bottom: 1px solid %1; font-weight: 600; }")
            .arg(stroke));
}

QString MainWindow::currentProfileName() const
{
    return m_previewProfile.isEmpty() ? m_configMgr.activeProfile() : m_previewProfile;
}
QByteArray MainWindow::configFingerprint() const
{
    QByteArray result = m_configMgr.activeProfile().toUtf8() + "\n";
    for (const QString &name : m_configMgr.profileNames()) {
        result += name.toUtf8() + "\n";
        for (const CurvePoint &point : m_configMgr.profile(name).points)
            result += QByteArray::number(point.first, 'g', 16) + "," +
                      QByteArray::number(point.second, 'g', 16) + ";";
        result += '\n';
    }
    return result;
}
void MainWindow::showPage(int index) { m_pages->setCurrentIndex(index); }

void MainWindow::showStatus(const QString &text)
{
    if (m_statusMessage->text() == text)
        return;
    m_statusMessage->setText(text);
    m_statusMessage->setAccessibleDescription(text);
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
    QAccessibleAnnouncementEvent event(m_statusMessage, text);
#else
    QAccessibleEvent event(m_statusMessage, QAccessible::Alert);
#endif
    QAccessible::updateAccessibility(&event);
}

void MainWindow::setupSessionTools()
{
    m_pages->widget(0)->setObjectName("workspace");
    m_configuredValue->setObjectName("configuredProfile");
    auto *diagnostics = m_pages->widget(2);
    auto *diagnosticsLayout = qobject_cast<QVBoxLayout *>(diagnostics->layout());
    m_history = new SessionHistory(diagnostics);
    diagnosticsLayout->insertWidget(diagnosticsLayout->count() - 1, m_history, 1);
    auto *settings = qobject_cast<QVBoxLayout *>(m_pages->widget(1)->layout());
    auto *login = new QCheckBox(tr("Open Lenovo Fan Boost at login"), m_pages->widget(1));
    login->setObjectName("startAtLogin");
    login->setChecked(Autostart::enabled());
    login->setEnabled(!m_previewMode);
    login->setAccessibleDescription(
        tr("Opens the application in the tray. Does not change fan control or the system service."));
    settings->insertWidget(settings->count() - 1, login);
    connect(login, &QCheckBox::toggled, this, [this, login](bool enabled) {
        QString error;
        if (!Autostart::setEnabled(enabled, &error)) {
            const QSignalBlocker blocker(login);
            login->setChecked(Autostart::enabled());
            showStatus(error);
        } else
            showStatus(enabled ? tr("Application will open at login.")
                               : tr("Application login startup disabled."));
    });
    connect(qApp, &QApplication::focusChanged, this, [this](QWidget *, QWidget *focused) {
        if (!focused || !isAncestorOf(focused))
            return;
        for (auto *parent = focused->parentWidget(); parent; parent = parent->parentWidget()) {
            if (auto *scroll = qobject_cast<QScrollArea *>(parent))
                scroll->ensureWidgetVisible(focused, 12, 12);
        }
    });
    connect(qApp->styleHints(), &QStyleHints::colorSchemeChanged, this, [this] {
        if (m_theme == "system")
            applyTheme(m_theme);
    });
}

void MainWindow::updateStateLayout()
{
    if (!m_headingLayout || !m_readingsLayout)
        return;
    while (auto *item = m_headingLayout->takeAt(0))
        delete item;
    while (auto *item = m_readingsLayout->takeAt(0))
        delete item;
    const bool compact = width() < 1000 || fontMetrics().height() > 20;
    m_headingLayout->addWidget(m_headingNames, 0, 0);
    m_headingLayout->addWidget(m_headingActions, compact ? 1 : 0, compact ? 0 : 1, Qt::AlignLeft);
    m_headingLayout->setColumnStretch(0, 1);
    m_readingsLayout->addWidget(m_cpuCaption, 0, 0);
    m_readingsLayout->addWidget(m_tempValue, 0, 1);
    m_readingsLayout->addWidget(m_requestCaption, compact ? 1 : 0, compact ? 0 : 2);
    m_readingsLayout->addWidget(m_targetValue, compact ? 1 : 0, compact ? 1 : 3);
    m_readingsLayout->setColumnStretch(compact ? 2 : 4, 1);
    if (m_profiles->currentItem())
        m_profiles->scrollToItem(m_profiles->currentItem());
}

void MainWindow::updateBusyState()
{
    m_operationBusy = m_configMgr.isBusy() || m_telemetry.isBusy();
    m_profiles->setEnabled(!m_operationBusy);
    m_curveEditor->setEditable(!m_operationBusy);
    m_createProfileButton->setEnabled(!m_operationBusy);
    m_importProfileButton->setEnabled(!m_operationBusy && !m_previewMode);
    m_duplicateProfileButton->setEnabled(!m_operationBusy && !m_configMgr.isDefaultProfile(m_previewProfile));
    m_renameProfileButton->setEnabled(!m_operationBusy && !m_configMgr.isDefaultProfile(m_previewProfile));
    m_deleteProfileButton->setEnabled(!m_operationBusy && !m_configMgr.isDefaultProfile(m_previewProfile));
    m_exportProfileButton->setEnabled(!m_operationBusy && !m_previewMode &&
                                      !m_configMgr.isDefaultProfile(m_previewProfile));
    m_pages->setEnabled(!m_operationBusy);
    m_modeSelector->setEnabled(!m_operationBusy && !m_previewMode && m_serviceKnown && !m_modePending);
    m_refreshStatusButton->setEnabled(!m_operationBusy && !m_previewMode);
    if (auto *button = findChild<QPushButton *>("enableBoot"))
        button->setEnabled(!m_operationBusy && !m_previewMode);
    if (auto *button = findChild<QPushButton *>("disableBoot"))
        button->setEnabled(!m_operationBusy && !m_previewMode);
    m_trayIcon->setOperationsEnabled(!m_operationBusy);
    m_applyButton->setEnabled(m_isModified && !m_operationBusy);
    m_discardButton->setEnabled(m_isModified && !m_operationBusy);
}

void MainWindow::onModeSelected(int index)
{
    if (index < 0)
        return;
    requestMode(index);
}

void MainWindow::requestMode(int mode, bool explicitProfileUse)
{
    if (m_previewMode)
        return;
    if ((!m_serviceKnown && !explicitProfileUse) || mode < 0 || mode > 1) {
        const QSignalBlocker blocker(m_modeSelector);
        m_modeSelector->setCurrentIndex(-1);
        m_modeSelector->setPlaceholderText(tr("Checking service status"));
        showStatus(tr("Service state is unknown. Refreshing; no mode change was requested."));
        m_telemetry.refreshNow();
        return;
    }
    if (mode == m_confirmedMode) {
        syncModeSelector();
        return;
    }
    if (mode == 1 && m_isModified && !explicitProfileUse) {
        QMessageBox prompt(
            QMessageBox::Question, tr("Use the saved profile?"),
            tr("Start fan control with saved profile '%1'?\nYour edits to '%2' will remain unsaved.")
                .arg(m_configuredProfile, m_previewProfile),
            QMessageBox::NoButton, this);
        auto *useSaved = prompt.addButton(tr("Use saved profile"), QMessageBox::AcceptRole);
        prompt.addButton(QMessageBox::Cancel);
        prompt.exec();
        if (prompt.clickedButton() != useSaved) {
            syncModeSelector();
            return;
        }
    }
    m_modePending = true;
    m_modeOperationPending = true;
    m_pendingMode = mode;
    syncModeSelector();
    showStatus(mode == 1 ? tr("Requesting fan curve mode…") : tr("Requesting firmware automatic mode…"));
    m_telemetry.requestServiceActive(mode == 1);
}

void MainWindow::syncModeSelector()
{
    if (!m_modeSelector)
        return;
    const QSignalBlocker blocker(m_modeSelector);
    m_modeSelector->setCurrentIndex(m_modePending ? m_pendingMode : m_confirmedMode);
    m_modeSelector->setPlaceholderText(m_confirmedMode < 0 ? tr("Checking service status") : QString{});
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    QTimer::singleShot(0, this, [this] { updateStateLayout(); });
}

void MainWindow::updateProfileListUi()
{
    QSignalBlocker blocker(m_profiles);
    m_profiles->clear();
    for (const QString &name :
         {QStringLiteral("Quiet"), QStringLiteral("Balanced"), QStringLiteral("Aggressive")}) {
        if (!m_configMgr.profileNames().contains(name))
            continue;
        auto *i = new QListWidgetItem(name, m_profiles);
        i->setData(Qt::UserRole, name);
        i->setToolTip(tr("Preview this profile"));
        i->setIcon(profileIcon(name, palette().color(QPalette::Text)));
    }
    for (const QString &name : m_configMgr.profileNames())
        if (!m_configMgr.isDefaultProfile(name)) {
            auto *i = new QListWidgetItem(name, m_profiles);
            i->setData(Qt::UserRole, name);
            i->setIcon(profileIcon("Custom", palette().color(QPalette::Text)));
        }
    if (m_previewProfile.isEmpty())
        m_previewProfile = m_configMgr.activeProfile();
    const QString current = m_previewProfile;
    for (int i = 0; i < m_profiles->count(); ++i)
        if (m_profiles->item(i)->data(Qt::UserRole).toString() == current) {
            m_profiles->setCurrentRow(i);
            break;
        }
    m_editorTitle->setText(current);
    m_editorSubTitle->setText(current == m_configuredProfile ? tr("Configured profile")
                                                             : tr("Selected profile"));
    const CurvePoints points = m_configMgr.profile(current).points;
    if (m_curveEditor->points() != points)
        m_curveEditor->setPoints(points);
    updateProfileActions();
}

bool MainWindow::resolveDraft(const QString &nextProfile)
{
    if (!m_isModified)
        return true;
    QMessageBox box(QMessageBox::Question, tr("Unsaved changes"),
                    tr("Apply the current changes before switching profiles?"), QMessageBox::NoButton, this);
    auto *apply = box.addButton(tr("Save draft"), QMessageBox::AcceptRole);
    auto *discard = box.addButton(tr("Discard draft"), QMessageBox::DestructiveRole);
    auto *cancel = box.addButton(QMessageBox::Cancel);
    box.exec();
    if (box.clickedButton() == cancel)
        return false;
    if (box.clickedButton() == apply) {
        if (m_previewMode) {
            applyChanges();
            return true;
        }
        m_pendingProfile = nextProfile;
        applyChanges();
        return false;
    }
    onDiscardClicked();
    return true;
}


void MainWindow::selectProfile(const QString &name)
{
    if (m_operationBusy || name.isEmpty() || !m_configMgr.profileNames().contains(name)
        || name == m_previewProfile) return;
    if (hasProfileEdits() && !resolveDraft(name)) {
        updateProfileListUi();
        return;
    }
    if (!m_configMgr.profileNames().contains(name)) return;
    m_configMgr.setActiveProfile(name);
    m_previewProfile = name;
    updateProfileListUi();
    setModified(true);
}

bool MainWindow::hasProfileEdits() const
{
    const auto current = configFingerprint();
    return current.mid(current.indexOf('\n')+1)
        != m_savedFingerprint.mid(m_savedFingerprint.indexOf('\n')+1);
}

void MainWindow::updateProfileActions()
{
    const bool custom = !m_previewProfile.isEmpty() &&
                        m_configMgr.profileNames().contains(m_previewProfile) &&
                        !m_configMgr.isDefaultProfile(m_previewProfile);
    m_profileActions->setVisible(custom);
}

void MainWindow::createProfile()
{
    QDialog dialog(this);
    dialog.setWindowTitle(tr("Create profile"));
    auto *layout = new QVBoxLayout(&dialog);
    auto *form = new QFormLayout;
    auto *nameEdit = new QLineEdit(&dialog);
    nameEdit->setObjectName("profileName");
    nameEdit->setPlaceholderText(tr("For example: Quiet study"));
    auto *base = new QComboBox(&dialog);
    base->setObjectName("profileBase");
    for (const QString &name :
         {QStringLiteral("Quiet"), QStringLiteral("Balanced"), QStringLiteral("Aggressive")})
        base->addItem(name, name);
    form->addRow(tr("Profile name"), nameEdit);
    form->addRow(tr("Start from preset"), base);
    layout->addLayout(form);
    auto *currentCurve =
        new QCheckBox(tr("Use the currently previewed curve as the starting point"), &dialog);
    currentCurve->setObjectName("useCurrentCurve");
    currentCurve->setChecked(false);
    layout->addWidget(currentCurve);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Cancel | QDialogButtonBox::Ok, &dialog);
    buttons->button(QDialogButtonBox::Ok)->setText(tr("Create profile"));
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);
    if (dialog.exec() != QDialog::Accepted)
        return;
    const QString name = nameEdit->text().trimmed();
    if (name.isEmpty() || m_configMgr.profileNames().contains(name)) {
        QMessageBox::warning(this, tr("Profile name unavailable"),
                             tr("Enter a nonempty name that is not already in use."));
        return;
    }
    const QString baseName = base->currentData().toString();
    const CurvePoints points = currentCurve->isChecked() ? m_configMgr.profile(m_previewProfile).points
                                                         : m_configMgr.profile(baseName).points;
    if (ConfigManager::serializeToToml(name, 0.5, {{name, {name, points}}}).isEmpty()) {
        QMessageBox::warning(this, tr("Invalid profile name"),
                             tr("Use a name of 1 to 64 characters without control characters."));
        return;
    }
    m_configMgr.addOrUpdateProfile(name, points);
    if (!m_configMgr.profileNames().contains(name))
        return;
    m_configMgr.setActiveProfile(name);
    m_implicitCopySource.clear();
    m_implicitCopyName.clear();
    m_previewProfile = name;
    updateProfileListUi();
    setModified(true);
}

void MainWindow::duplicateProfile()
{
    const QString source = m_previewProfile;
    if (source.isEmpty() || m_configMgr.isDefaultProfile(source))
        return;
    bool ok = false;
    const QString name = QInputDialog::getText(this, tr("Duplicate profile"), tr("New profile name:"),
                                               QLineEdit::Normal, source + tr(" Copy"), &ok)
                             .trimmed();
    if (!ok || name.isEmpty())
        return;
    if (m_configMgr.profileNames().contains(name)) {
        QMessageBox::warning(this, tr("Profile name unavailable"),
                             tr("Choose a name that is not already in use."));
        return;
    }
    const CurvePoints points = m_configMgr.profile(source).points;
    if (ConfigManager::serializeToToml(name, 0.5, {{name, {name, points}}}).isEmpty()) {
        QMessageBox::warning(this, tr("Invalid profile name"),
                             tr("Use a name of 1 to 64 characters without control characters."));
        return;
    }
    m_configMgr.addOrUpdateProfile(name, points);
    if (!m_configMgr.profileNames().contains(name))
        return;
    m_configMgr.setActiveProfile(name);
    m_implicitCopySource.clear();
    m_implicitCopyName.clear();
    m_previewProfile = name;
    updateProfileListUi();
    setModified(true);
}

void MainWindow::renameProfile()
{
    const QString oldName = m_previewProfile;
    if (oldName.isEmpty() || m_configMgr.isDefaultProfile(oldName))
        return;
    bool ok = false;
    const QString name = QInputDialog::getText(this, tr("Rename profile"), tr("New profile name:"),
                                               QLineEdit::Normal, oldName, &ok)
                             .trimmed();
    if (!ok || name.isEmpty() || name == oldName)
        return;
    if (m_configMgr.profileNames().contains(name) || m_configMgr.isDefaultProfile(name)) {
        QMessageBox::warning(this, tr("Profile name unavailable"),
                             tr("Choose a name that is not already in use. Built-in names are reserved."));
        return;
    }
    const CurvePoints points = m_configMgr.profile(oldName).points;
    if (ConfigManager::serializeToToml(name, 0.5, {{name, {name, points}}}).isEmpty()) {
        QMessageBox::warning(this, tr("Invalid profile name"),
                             tr("Use a name of 1 to 64 characters without control characters."));
        return;
    }
    const bool wasActive = m_configMgr.activeProfile() == oldName;
    m_configMgr.addOrUpdateProfile(name, points);
    if (!m_configMgr.profileNames().contains(name))
        return;
    m_configMgr.removeProfile(oldName);
    if (wasActive)
        m_configMgr.setActiveProfile(name);
    if (m_implicitCopyName == oldName) {
        m_implicitCopyName.clear();
        m_implicitCopySource.clear();
    }
    m_previewProfile = name;
    updateProfileListUi();
    setModified(true);
}

void MainWindow::deleteProfile()
{
    const QString name = m_previewProfile;
    if (name.isEmpty() || m_configMgr.isDefaultProfile(name))
        return;
    QStringList replacements = m_configMgr.profileNames();
    replacements.removeAll(name);
    if (replacements.isEmpty())
        return;
    bool ok = false;
    const QString replacement = QInputDialog::getItem(
        this, tr("Delete profile"), tr("Choose a replacement profile:"), replacements, 0, false, &ok);
    if (!ok || replacement.isEmpty())
        return;
    if (QMessageBox::question(this, tr("Delete profile"),
                              tr("Remove '%1' from the pending configuration and select '%2'?\nThe change "
                                 "takes effect after saving the profile.")
                                  .arg(name, replacement),
                              QMessageBox::Yes | QMessageBox::Cancel) != QMessageBox::Yes)
        return;
    m_configMgr.setActiveProfile(replacement);
    m_configMgr.removeProfile(name);
    if (m_implicitCopyName == name) {
        m_implicitCopyName.clear();
        m_implicitCopySource.clear();
    }
    m_previewProfile = replacement;
    updateProfileListUi();
    setModified(true);
}

void MainWindow::onTelemetryUpdated(const TelemetryData &data)
{
    if (!m_historyClock.isValid() || m_historyClock.elapsed() >= 1000) {
        m_history->append(data);
        m_historyClock.start();
    }
    updateTelemetryUi(data);
}
void MainWindow::updateTelemetryUi(const TelemetryData &d)
{
    m_currentServiceActive = d.serviceActive;
    m_serviceKnown = d.serviceKnown && d.serviceAgeMs >= 0 && d.serviceAgeMs <= 6000;
    m_currentMaxRpm = d.maxRpmValid ? d.maxRpm : 0;
    const bool serviceStale = d.serviceKnown && (d.serviceAgeMs < 0 || d.serviceAgeMs > 6000);
    m_serviceValue->setText(!d.serviceKnown   ? tr("Status unknown")
                            : serviceStale    ? tr("Service status stale")
                            : d.serviceActive ? tr("Service running")
                            : d.serviceFailed ? tr("Service failed")
                                              : tr("Service stopped"));
    m_bootStatus->setText(tr("Boot startup status: %1")
                              .arg(!d.bootEnabledKnown ? tr("unknown")
                                   : d.bootEnabled     ? tr("enabled")
                                                       : tr("disabled")));
    m_confirmedMode = m_serviceKnown ? (d.serviceActive ? 1 : 0) : -1;
    if (m_modePending && m_confirmedMode == m_pendingMode) {
        m_modePending = false;
        m_pendingMode = -1;
        showStatus(m_confirmedMode == 1 ? tr("Fan curve mode confirmed.")
                                        : tr("Firmware automatic mode confirmed."));
    }
    syncModeSelector();
    m_modeSelector->setEnabled(!m_operationBusy && !m_previewMode && m_serviceKnown && !m_modePending);
    m_refreshStatusButton->setEnabled(!m_operationBusy && !m_previewMode);
    updateActionLabel();
    if (m_serviceKnown && d.serviceActive && d.targetValid && d.targetRpm == 0 && d.targetAgeMs >= 0 &&
        d.targetAgeMs <= 3000)
        m_serviceValue->setText(tr("Firmware control (curve monitoring)"));
    const bool tempStale = d.tempValid && d.tempAgeMs > 3000;
    const bool targetStale = d.targetValid && d.targetAgeMs > 3000;
    m_tempValue->setText(d.tempValid ? QString::number(d.tempCelsius, 'f', 1) +
                                           tr(" °C%1").arg(tempStale ? tr(" (stale)") : QString{})
                                     : tr("Unavailable"));
    m_targetValue->setText(
        d.targetValid ? (d.targetRpm == 0
                             ? tr("Firmware automatic%1").arg(targetStale ? tr(" (stale)") : QString{})
                             : tr("%1 RPM%2").arg(d.targetRpm).arg(targetStale ? tr(" (stale)") : QString{}))
                      : tr("Unavailable"));
    m_configuredValue->setText(m_configuredProfile.isEmpty() ? tr("Unknown") : m_configuredProfile);
    m_trayIcon->setConfiguredProfile(m_configuredProfile);
    m_curveEditor->setTelemetryAvailable(d.tempValid);
    if (d.tempValid)
        m_curveEditor->setCurrentTemperature(d.tempCelsius);
    m_trayIcon->updateTelemetry(d);
    m_diagnosticsText->setText(
        tr("Service: %1\nDriver: %2\nCPU temperature: %3\nRequested fan speed: %4\nMaximum RPM: %5\nRaw EC "
           "reading: %6\nConfigured profile: %7\nConfiguration: %8\nBoot startup: %9")
            .arg(!d.serviceKnown   ? tr("unknown")
                 : d.serviceActive ? tr("running")
                 : d.serviceFailed ? tr("failed")
                                   : tr("stopped"))
            .arg(d.driverLoaded ? tr("loaded") : tr("not detected"))
            .arg(d.tempValid ? QString::number(d.tempCelsius, 'f', 1) + tr(" °C") : tr("unavailable"))
            .arg(d.targetValid ? QString::number(d.targetRpm) + tr(" RPM") : tr("unavailable"))
            .arg(d.maxRpmValid ? QString::number(d.maxRpm) + tr(" RPM") : tr("unknown"))
            .arg(d.ecRawValid ? QString::number(d.ecRaw) : tr("unavailable"))
            .arg(m_configuredProfile.isEmpty() ? tr("unknown") : m_configuredProfile)
            .arg(QFileInfo("/etc/lenovo-fanboost.toml").exists() ? tr("present") : tr("not found"))
            .arg(!d.bootEnabledKnown ? tr("unknown")
                 : d.bootEnabled     ? tr("enabled")
                                     : tr("disabled")));
}

void MainWindow::onConfigLoaded()
{
    m_savedFingerprint = configFingerprint();
    updateProfileListUi();
    setModified(false);
    if (m_configuredProfile.isEmpty())
        showStatus(tr("Configuration could not be read. The saved profile is unknown."));
    else
        showStatus(tr("Configuration loaded."));
}

void MainWindow::onCurveModified(const CurvePoints &points)
{
    QString name = m_previewProfile;
    if (name == m_implicitCopyName && !m_implicitCopySource.isEmpty() &&
        points == m_configMgr.profile(m_implicitCopySource).points) {
        m_previewProfile = m_implicitCopySource;
        m_configMgr.setActiveProfile(m_implicitCopySource);
        m_configMgr.removeProfile(m_implicitCopyName);
        m_implicitCopyName.clear();
        m_implicitCopySource.clear();
        updateProfileListUi();
        setModified(false);
        return;
    }
    if (m_configMgr.isDefaultProfile(name)) {
        QString base = name + tr(" Custom"), unique = base;
        int n = 2;
        while (m_configMgr.profileNames().contains(unique))
            unique = base + QStringLiteral(" %1").arg(n++);
        m_implicitCopySource = name;
        m_implicitCopyName = unique;
        m_configMgr.addOrUpdateProfile(unique, points);
        m_configMgr.setActiveProfile(unique);
        m_previewProfile = unique;
        updateProfileListUi();
    } else {
        m_configMgr.setActiveProfile(name);
        m_configMgr.addOrUpdateProfile(name, points);
    }
    m_editorTitle->setText(m_configMgr.activeProfile());
    m_editorSubTitle->setText(tr("Draft (not saved)"));
    setModified(true);
}

void MainWindow::setModified(bool modified)
{
    Q_UNUSED(modified);
    m_isModified = configFingerprint() != m_savedFingerprint;
    m_applyButton->setEnabled(m_isModified && !m_operationBusy);
    m_discardButton->setEnabled(m_isModified && !m_operationBusy);
    updateActionLabel();
    if (m_isModified) {
        showStatus(
            m_previewMode
                ? tr("Preview changes pending in memory.")
                : tr("Changes pending. The running service may still use the previous configuration."));
        m_editorSubTitle->setText(hasProfileEdits() ? tr("Draft (not saved)") : tr("Selected profile (not applied)"));
    } else {
        showStatus(m_previewMode ? tr("Preview sample data. System operations are disabled.")
                                 : tr("No pending changes."));
        m_editorSubTitle->setText(m_previewProfile == m_configuredProfile ? tr("Configured profile")
                                                                          : tr("Selected profile"));
    }
    updateProfileActions();
}

void MainWindow::updateActionLabel()
{
    const QString profile =
        m_configMgr.activeProfile().isEmpty() ? tr("profile") : m_configMgr.activeProfile();
    const QString label = m_serviceKnown && m_currentServiceActive ? tr("Save and use %1") : tr("Save %1");
    const QString displayed = m_applyButton->fontMetrics().elidedText(profile, Qt::ElideMiddle, 180);
    m_applyButton->setText(label.arg(displayed));
    m_applyButton->setAccessibleName(label.arg(profile));
    m_applyButton->setToolTip(label.arg(profile));
}

void MainWindow::applyChanges()
{
    if (m_operationBusy || !m_isModified)
        return;
    if (m_previewMode) {
        m_configuredProfile = m_configMgr.activeProfile();
        m_configuredValue->setText(m_configuredProfile);
        m_trayIcon->setConfiguredProfile(m_configuredProfile);
        m_savedFingerprint = configFingerprint();
        setModified(false);
        showStatus(tr("Preview updated in memory. No system configuration was changed."));
        return;
    }
    m_operationBusy = true;
    m_savingProfile = m_configMgr.activeProfile();
    m_applyButton->setEnabled(false);
    m_discardButton->setEnabled(false);
    showStatus(tr("Saving %1…").arg(m_savingProfile));
    m_configMgr.saveAsync();
}
void MainWindow::onSaveClicked() { applyChanges(); }
void MainWindow::onDiscardClicked()
{
    if (m_previewMode) {
        for (const QString &name : m_configMgr.profileNames())
            if (!m_configMgr.isDefaultProfile(name))
                m_configMgr.removeProfile(name);
        m_configMgr.addOrUpdateProfile("Quiet", ConfigManager::defaultQuietPoints());
        m_configMgr.addOrUpdateProfile("Balanced",
                                       {{40, 0}, {50, 0}, {60, 45}, {70, 65}, {80, 85}, {90, 100}});
        m_configMgr.addOrUpdateProfile("Aggressive", ConfigManager::defaultAggressivePoints());
        m_configMgr.addOrUpdateProfile("Custom",
                                       {{40, 0}, {50, 10}, {60, 40}, {70, 65}, {80, 85}, {90, 100}});
        m_configMgr.setActiveProfile("Balanced");
        m_previewProfile = "Balanced";
    } else {
        const bool loaded = m_configMgr.load();
        m_configuredProfile = loaded ? m_configMgr.activeProfile() : QString{};
        m_previewProfile = m_configMgr.activeProfile();
        m_previewProfile = m_configuredProfile;
    }
    if (m_previewMode || !m_configuredProfile.isEmpty())
        m_savedFingerprint = configFingerprint();
    m_configuredValue->setText(m_configuredProfile.isEmpty() ? tr("Unknown") : m_configuredProfile);
    m_trayIcon->setConfiguredProfile(m_configuredProfile);
    updateProfileListUi();
    setModified(false);
    showStatus(m_previewMode ? tr("Preview changes discarded in memory.")
                             : tr("Changes discarded; saved configuration reloaded."));
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (m_operationBusy) {
        showStatus(tr("Wait for the pending operation to finish before closing."));
        event->ignore();
        return;
    }
    if (m_isModified) {
        QMessageBox box(QMessageBox::Question, tr("Unsaved changes"),
                        tr("Save the current draft before closing?"), QMessageBox::NoButton, this);
        auto *apply = box.addButton(tr("Save draft"), QMessageBox::AcceptRole);
        auto *discard = box.addButton(tr("Discard draft"), QMessageBox::DestructiveRole);
        auto *cancel = box.addButton(QMessageBox::Cancel);
        box.exec();
        if (box.clickedButton() == cancel) {
            event->ignore();
            return;
        }
        if (box.clickedButton() == apply) {
            m_pendingProfile.clear();
            m_closingAfterApply = true;
            applyChanges();
            event->ignore();
            return;
        }
        Q_UNUSED(discard);
        onDiscardClicked();
    }
    if (!m_previewMode) {
        m_settings.setValue("window/closeToTray", m_closeToTray->isChecked());
        m_settings.setValue("window/geometry", saveGeometry());
    }
    if (m_closeToTray->isChecked() && m_trayIcon->isVisible() && QSystemTrayIcon::isSystemTrayAvailable()) {
        hide();
        event->ignore();
    } else {
        m_telemetry.stop();
        event->accept();
    }
}
