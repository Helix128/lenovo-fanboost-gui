#include "session_history.h"

#include <QDateTime>
#include <QFrame>
#include <QHeaderView>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QScrollArea>
#include <QSizePolicy>
#include <QTableWidget>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>
#include <limits>

namespace
{
constexpr int maxSamples = 600;

class HistoryPlot final : public QWidget
{
  public:
    HistoryPlot(SessionHistory *history, bool temperature, QWidget *parent = nullptr)
        : QWidget(parent), m_history(history), m_temperature(temperature)
    {
        setMinimumHeight(200);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::MinimumExpanding);
        setAccessibleName(temperature ? tr("CPU temperature history") : tr("Requested fan speed history"));
    }

  protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        const QRectF area = rect().adjusted(48, 12, -12, -26);
        const QPalette pal = palette();
        painter.fillRect(rect(), pal.color(QPalette::Base));
        painter.setPen(QPen(pal.color(QPalette::Mid), 1));
        painter.drawRect(area);

        const int count = m_history->sampleCount();
        if (!count) {
            painter.setPen(pal.color(QPalette::PlaceholderText));
            painter.drawText(area, Qt::AlignCenter, tr("Waiting for telemetry"));
            return;
        }

        // The plot obtains immutable display values from the widget's public sample accessor.
        const QVector<SessionHistory::Sample> history = m_history->samplesForPlot();
        double minimum = std::numeric_limits<double>::infinity();
        double maximum = -std::numeric_limits<double>::infinity();
        for (const auto &sample : history) {
            const bool valid = m_temperature ? sample.temperatureValid : sample.requestedRpmValid;
            if (!valid)
                continue;
            const double value = m_temperature ? sample.temperature : sample.requestedRpm;
            minimum = std::min(minimum, value);
            maximum = std::max(maximum, value);
        }
        if (!std::isfinite(minimum)) {
            painter.setPen(pal.color(QPalette::PlaceholderText));
            painter.drawText(area, Qt::AlignCenter, tr("Telemetry unavailable"));
            return;
        }
        if (maximum - minimum < 1.0) {
            minimum -= 1.0;
            maximum += 1.0;
        }
        const double padding = (maximum - minimum) * 0.08;
        minimum -= padding;
        maximum += padding;

        painter.setPen(pal.color(QPalette::PlaceholderText));
        painter.drawText(QRectF(0, area.top(), 42, 20), Qt::AlignRight | Qt::AlignVCenter,
                         m_temperature ? tr("°C") : tr("RPM"));
        painter.drawText(QRectF(area.left(), area.bottom() + 4, area.width(), 18), Qt::AlignCenter,
                         tr("Oldest                         Newest"));

        QPainterPath path;
        bool started = false;
        int previousIndex = -2;
        const int last = history.size() - 1;
        for (int i = 0; i < history.size(); ++i) {
            const auto &sample = history[i];
            const bool valid = m_temperature ? sample.temperatureValid : sample.requestedRpmValid;
            if (!valid) {
                started = false;
                continue;
            }
            const double value = m_temperature ? sample.temperature : sample.requestedRpm;
            const qreal x = area.left() + (last > 0 ? static_cast<qreal>(i) / last : 0.5) * area.width();
            const qreal y = area.bottom() - (value - minimum) / (maximum - minimum) * area.height();
            if (!started || i != previousIndex + 1)
                path.moveTo(x, y);
            else
                path.lineTo(x, y);
            started = true;
            previousIndex = i;
        }
        painter.setPen(QPen(pal.color(QPalette::Link), 2.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawPath(path);
    }

  private:
    SessionHistory *m_history;
    bool m_temperature;
};
} // namespace

SessionHistory::SessionHistory(QWidget *parent) : QWidget(parent)
{
    setObjectName("sessionHistory");
    setAccessibleName(tr("Session history"));
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    auto *heading = new QWidget(this);
    auto *headingLayout = new QVBoxLayout(heading);
    headingLayout->setContentsMargins(0, 0, 0, 0);
    auto *title = new QLabel(tr("Session history"), heading);
    title->setObjectName("pageTitle");
    m_summary = new QLabel(tr("No telemetry samples yet."), heading);
    m_summary->setObjectName("sessionHistorySummary");
    m_summary->setWordWrap(true);
    headingLayout->addWidget(title);
    headingLayout->addWidget(m_summary);
    layout->addWidget(heading);

    auto *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    auto *content = new QWidget(scroll);
    auto *contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(0, 8, 8, 8);

    auto *temperatureTitle = new QLabel(tr("CPU temperature"), content);
    m_temperaturePlot = new HistoryPlot(this, true, content);
    m_temperaturePlot->setObjectName("temperatureHistoryPlot");
    m_temperaturePlot->setAccessibleDescription(
        tr("CPU temperature samples over this session. Gaps indicate unavailable readings."));
    contentLayout->addWidget(temperatureTitle);
    contentLayout->addWidget(m_temperaturePlot);

    auto *requestedTitle = new QLabel(tr("Requested fan speed"), content);
    m_requestedPlot = new HistoryPlot(this, false, content);
    m_requestedPlot->setObjectName("requestedSpeedHistoryPlot");
    m_requestedPlot->setAccessibleDescription(tr(
        "Requested fan speed in RPM. This is not a measured fan speed. Gaps indicate unavailable readings."));
    contentLayout->addWidget(requestedTitle);
    contentLayout->addWidget(m_requestedPlot);

    auto *tableTitle = new QLabel(tr("Samples"), content);
    contentLayout->addWidget(tableTitle);
    m_table = new QTableWidget(content);
    m_table->setObjectName("sessionHistoryTable");
    m_table->setAccessibleName(tr("Session telemetry samples"));
    m_table->setAccessibleDescription(
        tr("Timestamp, CPU temperature, and requested fan speed. Unavailable values are shown explicitly."));
    m_table->setColumnCount(3);
    m_table->setHorizontalHeaderLabels({tr("Time"), tr("CPU temperature (°C)"), tr("Requested speed (RPM)")});
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    m_table->horizontalHeader()->setMinimumSectionSize(105);
    m_table->horizontalHeader()->setStretchLastSection(false);
    m_table->setColumnWidth(0, 150);
    m_table->setColumnWidth(1, 190);
    m_table->setColumnWidth(2, 210);
    m_table->verticalHeader()->setVisible(false);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setTabKeyNavigation(false);
    m_table->setMinimumHeight(200);
    contentLayout->addWidget(m_table);
    scroll->setWidget(content);
    layout->addWidget(scroll, 1);

    m_clearButton = new QPushButton(tr("Clear history"), this);
    m_clearButton->setObjectName("clearSessionHistory");
    connect(m_clearButton, &QPushButton::clicked, this, &SessionHistory::clear);
    layout->addWidget(m_clearButton, 0, Qt::AlignRight);
    refreshView();
}

void SessionHistory::append(const TelemetryData &data)
{
    Sample sample;
    sample.timestamp = QDateTime::currentDateTime();
    sample.temperature = data.tempCelsius;
    sample.requestedRpm = data.targetRpm;
    sample.temperatureValid = data.tempValid && std::isfinite(data.tempCelsius);
    sample.requestedRpmValid = data.targetValid && data.targetRpm >= 0;
    m_samples.append(sample);
    if (m_samples.size() > maxSamples)
        m_samples.remove(0, m_samples.size() - maxSamples);
    refreshView();
}

int SessionHistory::sampleCount() const { return m_samples.size(); }
QVector<SessionHistory::Sample> SessionHistory::samplesForPlot() const { return m_samples; }

void SessionHistory::clear()
{
    m_samples.clear();
    refreshView();
}

QString SessionHistory::formatTemperature(double value) const
{
    return QString::number(value, 'f', 1) + tr(" °C");
}

QString SessionHistory::formatRequestedRpm(int value) const { return QString::number(value) + tr(" RPM"); }

void SessionHistory::refreshView()
{
    const int count = m_samples.size();
    if (!count) {
        m_summary->setText(tr("No telemetry samples yet."));
    } else {
        double minimum = std::numeric_limits<double>::infinity();
        double maximum = -std::numeric_limits<double>::infinity();
        double current = 0;
        for (const auto &sample : m_samples) {
            if (!sample.temperatureValid)
                continue;
            current = sample.temperature;
            minimum = std::min(minimum, sample.temperature);
            maximum = std::max(maximum, sample.temperature);
        }
        if (!std::isfinite(minimum)) {
            m_summary->setText(tr("%1 samples · CPU temperature unavailable").arg(count));
        } else {
            const auto &latest = m_samples.constLast();
            m_summary->setText(
                tr("%1 samples · CPU temperature: current %2, minimum %3, maximum %4")
                    .arg(count)
                    .arg(latest.temperatureValid ? formatTemperature(latest.temperature) : tr("Unavailable"),
                         formatTemperature(minimum), formatTemperature(maximum)));
        }
    }

    if (!count)
        m_table->setRowCount(0);
    else {
        if (m_table->rowCount() >= count)
            m_table->removeRow(0);
        const int row = count - 1;
        m_table->insertRow(row);
        const Sample &sample = m_samples[row];
        m_table->setItem(row, 0, new QTableWidgetItem(sample.timestamp.toString(QStringLiteral("HH:mm:ss"))));
        m_table->setItem(row, 1,
                         new QTableWidgetItem(sample.temperatureValid ? formatTemperature(sample.temperature)
                                                                      : tr("Unavailable")));
        m_table->setItem(row, 2,
                         new QTableWidgetItem(sample.requestedRpmValid
                                                  ? formatRequestedRpm(sample.requestedRpm)
                                                  : tr("Unavailable")));
    }
    if (count)
        m_table->scrollToBottom();
    m_clearButton->setEnabled(count > 0);
    if (m_temperaturePlot->isVisible())
        m_temperaturePlot->update();
    if (m_requestedPlot->isVisible())
        m_requestedPlot->update();
}
