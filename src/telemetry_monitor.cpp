#include "telemetry_monitor.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QTextStream>

namespace
{
constexpr auto serviceName = "lenovo-fanboost.service";
constexpr int servicePollIntervalMs = 3000;
} // namespace

TelemetryMonitor::TelemetryMonitor(QObject *parent) : QObject(parent)
{
    connect(&m_timer, &QTimer::timeout, this, &TelemetryMonitor::poll);
    m_serviceTimer.setInterval(servicePollIntervalMs);
    m_serviceTimeout.setSingleShot(true);
    m_operationTimeout.setSingleShot(true);
    connect(&m_serviceTimeout, &QTimer::timeout, &m_serviceProcess, &QProcess::kill);
    connect(&m_operationTimeout, &QTimer::timeout, this, [this] {
        m_operationTimedOut = true;
        m_operationProcess.kill();
    });
    connect(&m_serviceTimer, &QTimer::timeout, this, &TelemetryMonitor::queryService);
    connect(&m_serviceProcess, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this,
            &TelemetryMonitor::handleServiceQueryFinished);
    connect(&m_operationProcess, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this,
            [this](int code, QProcess::ExitStatus status) {
                m_operationTimeout.stop();
                handleOperationFinished(code, status);
            });
    connect(&m_operationProcess, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart && m_operationBusy) {
            m_operationTimeout.stop();
            setOperationBusy(false);
            emit operationFinished(false,
                                   "The service operation was denied, cancelled, or could not be started.");
        }
    });
}

void TelemetryMonitor::start(int intervalMs)
{
    refreshNow();
    m_timer.start(qMax(intervalMs, 250));
    m_serviceTimer.start();
    queryService();
}

void TelemetryMonitor::stop()
{
    m_timer.stop();
    m_serviceTimer.stop();
}

void TelemetryMonitor::refreshNow()
{
    poll();
    queryService();
}

QString TelemetryMonitor::findHwmon(const QString &driverName) const
{
    QDir hwmonDir("/sys/class/hwmon");
    const QStringList entries = hwmonDir.entryList({"hwmon*"}, QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString &entry : entries) {
        QFile file(hwmonDir.filePath(entry) + "/name");
        if (file.open(QIODevice::ReadOnly | QIODevice::Text) &&
            QTextStream(&file).readLine().trimmed() == driverName)
            return hwmonDir.filePath(entry);
    }
    return {};
}

bool TelemetryMonitor::tryReadIntFromFile(const QString &path, int *value) const
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;
    bool ok = false;
    const int parsed = QTextStream(&file).readLine().trimmed().toInt(&ok);
    if (ok)
        *value = parsed;
    return ok;
}

void TelemetryMonitor::poll()
{
    if (m_fanHwmonPath.isEmpty() || !QFile::exists(m_fanHwmonPath))
        m_fanHwmonPath = findHwmon("lenovo_fanboost");
    if (m_tempHwmonPath.isEmpty() || !QFile::exists(m_tempHwmonPath))
        m_tempHwmonPath = findHwmon("k10temp");

    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    TelemetryData data = m_lastData;
    data.driverLoaded = !m_fanHwmonPath.isEmpty();

    int raw = 0;
    if (!m_tempHwmonPath.isEmpty() && tryReadIntFromFile(m_tempHwmonPath + "/temp1_input", &raw)) {
        data.tempCelsius = raw / 1000.0;
        data.tempValid = true;
        m_tempReadAt = now;
    } else {
        data.tempValid = false;
    }

    if (data.driverLoaded && tryReadIntFromFile(m_fanHwmonPath + "/fan1_target", &raw)) {
        data.targetRpm = raw;
        data.targetValid = raw >= 0;
        if (data.targetValid)
            m_targetReadAt = now;
    } else {
        data.targetValid = false;
    }
    if (data.driverLoaded && tryReadIntFromFile(m_fanHwmonPath + "/fan1_max", &raw)) {
        data.maxRpm = raw;
        data.maxRpmValid = raw > 0;
    } else {
        data.maxRpmValid = false;
    }
    if (data.driverLoaded && tryReadIntFromFile(m_fanHwmonPath + "/fan1_raw", &raw)) {
        data.ecRaw = raw;
        data.ecRawValid = true;
    } else {
        data.ecRawValid = false;
    }
    data.tempAgeMs = m_tempReadAt ? qMax<qint64>(0, now - m_tempReadAt) : -1;
    data.targetAgeMs = m_targetReadAt ? qMax<qint64>(0, now - m_targetReadAt) : -1;
    data.serviceAgeMs = m_serviceReadAt ? qMax<qint64>(0, now - m_serviceReadAt) : -1;
    m_lastData = data;
    emit telemetryUpdated(data);
}

void TelemetryMonitor::queryService()
{
    if (m_serviceProcess.state() != QProcess::NotRunning)
        return;
    m_serviceProcess.start("systemctl", {"show", "--property=LoadState", "--property=ActiveState",
                                         "--property=SubState", "--property=UnitFileState", serviceName});
    m_serviceTimeout.start(1500);
}

void TelemetryMonitor::handleServiceQueryFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    m_serviceTimeout.stop();
    if (exitStatus != QProcess::NormalExit || exitCode != 0) {
        m_lastData.serviceKnown = false;
        m_lastData.bootEnabledKnown = false;
        m_lastData.serviceAgeMs =
            m_serviceReadAt ? qMax<qint64>(0, QDateTime::currentMSecsSinceEpoch() - m_serviceReadAt) : -1;
        emit telemetryUpdated(m_lastData);
        return;
    }
    const QString output = QString::fromLocal8Bit(m_serviceProcess.readAllStandardOutput());
    QString loadState;
    QString activeState;
    QString subState;
    QString unitFileState;
    for (const QString &line : output.split('\n')) {
        if (line.startsWith("LoadState="))
            loadState = line.mid(10).trimmed();
        else if (line.startsWith("ActiveState="))
            activeState = line.mid(12).trimmed();
        else if (line.startsWith("SubState="))
            subState = line.mid(9).trimmed();
        else if (line.startsWith("UnitFileState="))
            unitFileState = line.mid(14).trimmed();
    }
    if (activeState.isEmpty() || loadState != "loaded" || unitFileState.isEmpty() || unitFileState == "bad") {
        m_lastData.serviceKnown = false;
        m_lastData.bootEnabledKnown = false;
        m_lastData.serviceAgeMs =
            m_serviceReadAt ? qMax<qint64>(0, QDateTime::currentMSecsSinceEpoch() - m_serviceReadAt) : -1;
        emit telemetryUpdated(m_lastData);
        return;
    }
    m_lastData.serviceKnown = true;
    m_lastData.bootEnabledKnown = true;
    m_lastData.bootEnabled = unitFileState == "enabled" || unitFileState == "enabled-runtime";
    m_lastData.serviceActive = activeState == "active";
    m_lastData.serviceFailed = activeState == "failed" || subState == "failed";
    m_serviceReadAt = QDateTime::currentMSecsSinceEpoch();
    m_lastData.serviceAgeMs = 0;
    emit telemetryUpdated(m_lastData);
}

void TelemetryMonitor::setOperationBusy(bool busy)
{
    if (m_operationBusy == busy)
        return;
    m_operationBusy = busy;
    emit busyChanged(busy);
}

void TelemetryMonitor::startOperation(const QStringList &arguments)
{
    if (m_operationBusy || m_operationProcess.state() != QProcess::NotRunning) {
        emit operationFinished(false, "A service operation is already in progress.");
        return;
    }
    setOperationBusy(true);
    m_operationTimedOut = false;
    m_operationTimeout.start(60000);
    m_operationProcess.start("pkexec", QStringList{"systemctl"} + arguments + QStringList{serviceName});
}

void TelemetryMonitor::requestServiceActive(bool active) { startOperation({active ? "start" : "stop"}); }

void TelemetryMonitor::requestStartAtBoot(bool enabled) { startOperation({enabled ? "enable" : "disable"}); }

void TelemetryMonitor::handleOperationFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    if (!m_operationBusy)
        return;
    const QString error = QString::fromLocal8Bit(m_operationProcess.readAllStandardError()).trimmed();
    const QString output = QString::fromLocal8Bit(m_operationProcess.readAllStandardOutput()).trimmed();
    const bool success = !m_operationTimedOut && exitStatus == QProcess::NormalExit && exitCode == 0;
    setOperationBusy(false);
    emit operationFinished(success, m_operationTimedOut ? "Service operation timed out."
                                    : success           ? "Service operation completed."
                                                        : (error.isEmpty() ? output : error));
    if (success)
        queryService();
}
