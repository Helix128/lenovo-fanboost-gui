#pragma once

#include <QObject>
#include <QTimer>
#include <QString>

struct TelemetryData {
    double tempCelsius{0.0};
    int targetRpm{0};
    int maxRpm{4000};
    int ecRaw{0};
    bool serviceActive{false};
    bool serviceFailed{false};
    bool driverLoaded{false};
};

class TelemetryMonitor : public QObject {
    Q_OBJECT
public:
    explicit TelemetryMonitor(QObject *parent = nullptr);
    ~TelemetryMonitor() override = default;

    void start(int intervalMs = 500);
    void stop();
    void refreshNow();

    static bool setServiceActive(bool active, QString *errorMessage = nullptr);

signals:
    void telemetryUpdated(const TelemetryData &data);

private slots:
    void poll();

private:
    QString findHwmon(const QString &driverName) const;
    int readIntFromFile(const QString &path, int fallback = 0) const;

    QTimer m_timer;
    QString m_fanHwmonPath;
    QString m_tempHwmonPath;
};
