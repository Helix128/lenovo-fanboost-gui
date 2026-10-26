#pragma once

#include <QObject>
#include <QProcess>
#include <QString>
#include <QStringList>
#include <QTimer>

struct TelemetryData {
    double tempCelsius{0.0};
    int targetRpm{0};
    int maxRpm{4000};
    int ecRaw{0};
    bool tempValid{false};
    bool targetValid{false};
    bool maxRpmValid{false};
    bool ecRawValid{false};
    bool serviceKnown{false};
    bool bootEnabledKnown{false};
    bool bootEnabled{false};
    qint64 tempAgeMs{-1};
    qint64 targetAgeMs{-1};
    qint64 serviceAgeMs{-1};
    bool serviceActive{false};
    bool serviceFailed{false};
    bool driverLoaded{false};
};

class TelemetryMonitor : public QObject
{
    Q_OBJECT
  public:
    explicit TelemetryMonitor(QObject *parent = nullptr);
    ~TelemetryMonitor() override = default;

    void start(int intervalMs = 500);
    void stop();
    void refreshNow();
    void requestServiceActive(bool active);
    void requestStartAtBoot(bool enabled);
    bool isBusy() const { return m_operationBusy; }

  signals:
    void telemetryUpdated(const TelemetryData &data);
    void operationFinished(bool success, const QString &message);
    void busyChanged(bool busy);

  private slots:
    void poll();
    void queryService();
    void handleServiceQueryFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void handleOperationFinished(int exitCode, QProcess::ExitStatus exitStatus);

  private:
    QString findHwmon(const QString &driverName) const;
    bool tryReadIntFromFile(const QString &path, int *value) const;
    void setOperationBusy(bool busy);
    void startOperation(const QStringList &arguments);

    QTimer m_timer;
    QTimer m_serviceTimer;
    QTimer m_serviceTimeout;
    QTimer m_operationTimeout;
    QString m_fanHwmonPath;
    QString m_tempHwmonPath;
    QProcess m_serviceProcess;
    QProcess m_operationProcess;
    TelemetryData m_lastData;
    qint64 m_tempReadAt{0};
    qint64 m_targetReadAt{0};
    qint64 m_serviceReadAt{0};
    bool m_operationBusy{false};
    bool m_operationTimedOut{false};
};
