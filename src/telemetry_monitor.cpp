#include "telemetry_monitor.h"

#include <QDir>
#include <QFile>
#include <QProcess>
#include <QTextStream>

TelemetryMonitor::TelemetryMonitor(QObject *parent)
    : QObject(parent)
{
    connect(&m_timer, &QTimer::timeout, this, &TelemetryMonitor::poll);
}

void TelemetryMonitor::start(int intervalMs)
{
    refreshNow();
    m_timer.start(intervalMs);
}

void TelemetryMonitor::stop()
{
    m_timer.stop();
}

void TelemetryMonitor::refreshNow()
{
    poll();
}

QString TelemetryMonitor::findHwmon(const QString &driverName) const
{
    QDir hwmonDir("/sys/class/hwmon");
    const QStringList entries = hwmonDir.entryList(QStringList() << "hwmon*", QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString &entry : entries) {
        QString namePath = hwmonDir.filePath(entry) + "/name";
        QFile file(namePath);
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QString content = QTextStream(&file).readLine().trimmed();
            if (content == driverName) {
                return hwmonDir.filePath(entry);
            }
        }
    }
    return QString();
}

int TelemetryMonitor::readIntFromFile(const QString &path, int fallback) const
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return fallback;
    }
    bool ok = false;
    int val = QTextStream(&file).readLine().trimmed().toInt(&ok);
    return ok ? val : fallback;
}

void TelemetryMonitor::poll()
{
    if (m_fanHwmonPath.isEmpty() || !QFile::exists(m_fanHwmonPath)) {
        m_fanHwmonPath = findHwmon("lenovo_fanboost");
    }
    if (m_tempHwmonPath.isEmpty() || !QFile::exists(m_tempHwmonPath)) {
        m_tempHwmonPath = findHwmon("k10temp");
    }

    TelemetryData data;
    data.driverLoaded = !m_fanHwmonPath.isEmpty();

    if (!m_tempHwmonPath.isEmpty()) {
        int rawTemp = readIntFromFile(m_tempHwmonPath + "/temp1_input", 0);
        data.tempCelsius = rawTemp / 1000.0;
    }

    if (data.driverLoaded) {
        data.targetRpm = readIntFromFile(m_fanHwmonPath + "/fan1_target", 0);
        data.maxRpm = readIntFromFile(m_fanHwmonPath + "/fan1_max", 4000);
        data.ecRaw = readIntFromFile(m_fanHwmonPath + "/fan1_raw", 0);
    }

    // Check systemd service status
    QProcess proc;
    proc.start("systemctl", QStringList() << "is-active" << "lenovo-fanboost.service");
    proc.waitForFinished(500);
    QString status = proc.readAllStandardOutput().trimmed();
    data.serviceActive = (status == "active");

    QProcess failProc;
    failProc.start("systemctl", QStringList() << "is-failed" << "lenovo-fanboost.service");
    failProc.waitForFinished(500);
    data.serviceFailed = (failProc.readAllStandardOutput().trimmed() == "failed");

    emit telemetryUpdated(data);
}

bool TelemetryMonitor::setServiceActive(bool active, QString *errorMessage)
{
    QString verb = active ? "enable" : "disable";
    QProcess proc;
    proc.start("pkexec", QStringList() << "systemctl" << verb << "--now" << "lenovo-fanboost.service");
    proc.waitForFinished(10000);

    if (proc.exitStatus() != QProcess::NormalExit || proc.exitCode() != 0) {
        if (errorMessage) {
            *errorMessage = proc.readAllStandardError().trimmed();
            if (errorMessage->isEmpty()) {
                *errorMessage = proc.readAllStandardOutput().trimmed();
            }
            if (errorMessage->isEmpty()) {
                *errorMessage = QString("Command 'systemctl %1' failed with code %2").arg(verb).arg(proc.exitCode());
            }
        }
        return false;
    }

    if (active) {
        // Give daemon 300ms to start and read config
        QProcess checkProc;
        checkProc.start("systemctl", QStringList() << "is-active" << "lenovo-fanboost.service");
        checkProc.waitForFinished(1000);
        if (checkProc.readAllStandardOutput().trimmed() != "active") {
            QProcess journalProc;
            journalProc.start("journalctl", QStringList() << "-u" << "lenovo-fanboost.service" << "-n" << "4" << "--no-pager");
            journalProc.waitForFinished(1000);
            if (errorMessage) {
                *errorMessage = QString("Service failed to start:\n%1")
                                    .arg(journalProc.readAllStandardOutput().trimmed());
            }
            return false;
        }
    }

    return true;
}
