#pragma once

#include "telemetry_monitor.h"

#include <QDateTime>
#include <QVector>
#include <QWidget>

class QLabel;
class QPushButton;
class QTableWidget;

class SessionHistory final : public QWidget
{
    Q_OBJECT
  public:
    struct Sample {
        QDateTime timestamp;
        double temperature{0.0};
        int requestedRpm{0};
        bool temperatureValid{false};
        bool requestedRpmValid{false};
    };

    explicit SessionHistory(QWidget *parent = nullptr);

    void append(const TelemetryData &data);
    int sampleCount() const;
    QVector<Sample> samplesForPlot() const;
    void clear();

  private:
    void refreshView();
    QString formatTemperature(double value) const;
    QString formatRequestedRpm(int value) const;

    QVector<Sample> m_samples;
    QLabel *m_summary{nullptr};
    QWidget *m_temperaturePlot{nullptr};
    QWidget *m_requestedPlot{nullptr};
    QTableWidget *m_table{nullptr};
    QPushButton *m_clearButton{nullptr};
};
