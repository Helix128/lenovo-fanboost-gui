#include "curve_editor.h"

#include <QContextMenuEvent>
#include <QLinearGradient>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QToolTip>
#include <algorithm>
#include <cmath>

CurveEditor::CurveEditor(QWidget *parent)
    : QWidget(parent)
{
    setMinimumSize(480, 260);
    setMouseTracking(true);
}

void CurveEditor::setPoints(const CurvePoints &points)
{
    m_points = points;
    std::sort(m_points.begin(), m_points.end(), [](const CurvePoint &a, const CurvePoint &b) {
        return a.first < b.first;
    });
    update();
}

CurvePoints CurveEditor::points() const
{
    return m_points;
}

void CurveEditor::setCurrentTemperature(double tempCelsius)
{
    m_currentTemp = tempCelsius;
    update();
}

double CurveEditor::currentTemperature() const
{
    return m_currentTemp;
}

void CurveEditor::setEditable(bool editable)
{
    m_editable = editable;
    update();
}

bool CurveEditor::isEditable() const
{
    return m_editable;
}

QRectF CurveEditor::plotArea() const
{
    const double leftMargin = 55.0;
    const double rightMargin = 25.0;
    const double topMargin = 30.0;
    const double bottomMargin = 40.0;
    return QRectF(leftMargin, topMargin,
                  width() - leftMargin - rightMargin,
                  height() - topMargin - bottomMargin);
}

QPointF CurveEditor::toCanvas(double temp, double percent) const
{
    const QRectF area = plotArea();
    double nx = (temp - m_minTemp) / (m_maxTemp - m_minTemp);
    double ny = (percent - m_minPct) / (m_maxPct - m_minPct);

    double x = area.left() + nx * area.width();
    double y = area.bottom() - ny * area.height();
    return QPointF(x, y);
}

CurvePoint CurveEditor::fromCanvas(const QPointF &pos) const
{
    const QRectF area = plotArea();
    double nx = (pos.x() - area.left()) / area.width();
    double ny = (area.bottom() - pos.y()) / area.height();

    double temp = m_minTemp + nx * (m_maxTemp - m_minTemp);
    double pct = m_minPct + ny * (m_maxPct - m_minPct);

    temp = std::clamp(temp, m_minTemp, m_maxTemp);
    pct = std::clamp(pct, m_minPct, m_maxPct);

    // Quantize temperature to 1°C steps and percent to 1%
    return {std::round(temp), std::round(pct)};
}

int CurveEditor::findPointNear(const QPointF &pos, double radius) const
{
    for (int i = 0; i < m_points.size(); ++i) {
        QPointF p = toCanvas(m_points[i].first, m_points[i].second);
        double dist = std::hypot(p.x() - pos.x(), p.y() - pos.y());
        if (dist <= radius) {
            return i;
        }
    }
    return -1;
}

double CurveEditor::interpolatePercentAt(double temp) const
{
    if (m_points.isEmpty()) return 0.0;
    if (temp <= m_points.first().first) return m_points.first().second;
    if (temp >= m_points.last().first) return m_points.last().second;

    for (int i = 0; i < m_points.size() - 1; ++i) {
        double t0 = m_points[i].first;
        double p0 = m_points[i].second;
        double t1 = m_points[i + 1].first;
        double p1 = m_points[i + 1].second;
        if (temp >= t0 && temp <= t1) {
            if (std::abs(t1 - t0) < 0.001) return p0;
            return p0 + ((temp - t0) / (t1 - t0)) * (p1 - p0);
        }
    }
    return 0.0;
}

void CurveEditor::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const QRectF area = plotArea();
    const QPalette pal = palette();

    // Background of plot area
    QColor bgColor = pal.color(QPalette::Base);
    p.fillRect(area, bgColor);

    // Subtle grid lines
    QColor gridColor = pal.color(QPalette::Midlight);
    gridColor.setAlpha(80);
    p.setPen(QPen(gridColor, 1.0, Qt::DashLine));

    QFont labelFont = font();
    labelFont.setPointSize(8);
    p.setFont(labelFont);
    p.setPen(pal.color(QPalette::PlaceholderText));

    // Y Axis: 0%, 20%, 40%, 60%, 80%, 100%
    for (int pct = 0; pct <= 100; pct += 20) {
        QPointF pt = toCanvas(m_minTemp, pct);
        p.drawLine(QPointF(area.left(), pt.y()), QPointF(area.right(), pt.y()));
        QString label = QString("%1%").arg(pct);
        if (pct == 0) {
            label = "0% (EC)";
        }
        p.drawText(QRectF(0, pt.y() - 8, area.left() - 8, 16), Qt::AlignRight | Qt::AlignVCenter, label);
    }

    // X Axis: 30°C to 100°C by 10s
    for (int t = 30; t <= 100; t += 10) {
        QPointF pt = toCanvas(t, m_minPct);
        p.drawLine(QPointF(pt.x(), area.top()), QPointF(pt.x(), area.bottom()));
        QString label = QString("%1°").arg(t);
        p.drawText(QRectF(pt.x() - 20, area.bottom() + 6, 40, 16), Qt::AlignHCenter | Qt::AlignTop, label);
    }

    // Axis titles
    QFont titleFont = font();
    titleFont.setPointSize(8);
    titleFont.setBold(true);
    p.setFont(titleFont);
    p.setPen(pal.color(QPalette::WindowText));
    p.drawText(QRectF(area.left(), area.bottom() + 22, area.width(), 16), Qt::AlignHCenter, "CPU Temperature (°C)");

    // Border around plot
    p.setPen(QPen(pal.color(QPalette::Mid), 1.0));
    p.setBrush(Qt::NoBrush);
    p.drawRect(area);

    if (m_points.size() < 2) {
        return;
    }

    // Curve Path & Shaded polygon below (with flat boundary extrapolation)
    QPainterPath path;
    QPainterPath fillPath;

    double firstPct = m_points.first().second;
    QPointF startExtrap = toCanvas(m_minTemp, firstPct);
    QPointF firstPt = toCanvas(m_points.first().first, firstPct);

    path.moveTo(startExtrap);
    path.lineTo(firstPt);

    fillPath.moveTo(QPointF(startExtrap.x(), area.bottom()));
    fillPath.lineTo(startExtrap);
    fillPath.lineTo(firstPt);

    for (int i = 1; i < m_points.size(); ++i) {
        QPointF pt = toCanvas(m_points[i].first, m_points[i].second);
        path.lineTo(pt);
        fillPath.lineTo(pt);
    }

    double lastPct = m_points.last().second;
    QPointF lastPt = toCanvas(m_points.last().first, lastPct);
    QPointF endExtrap = toCanvas(m_maxTemp, lastPct);

    path.lineTo(endExtrap);
    fillPath.lineTo(endExtrap);
    fillPath.lineTo(QPointF(endExtrap.x(), area.bottom()));
    fillPath.closeSubpath();

    // Gradient fill under curve
    QColor accentColor = pal.color(QPalette::Highlight);
    QLinearGradient fillGrad(0, area.top(), 0, area.bottom());
    QColor gradTop = accentColor;
    gradTop.setAlpha(70);
    QColor gradBottom = accentColor;
    gradBottom.setAlpha(10);
    fillGrad.setColorAt(0.0, gradTop);
    fillGrad.setColorAt(1.0, gradBottom);

    p.fillPath(fillPath, fillGrad);

    // Draw main curve line
    p.setPen(QPen(accentColor, 2.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.drawPath(path);

    // Live Temperature indicator
    if (m_currentTemp >= m_minTemp && m_currentTemp <= m_maxTemp) {
        double curPct = interpolatePercentAt(m_currentTemp);
        QPointF curPt = toCanvas(m_currentTemp, curPct);

        // Dashed vertical line
        QColor liveColor = (m_currentTemp >= 80.0) ? QColor("#da4453") :
                           (m_currentTemp >= 65.0) ? QColor("#f67400") : QColor("#27ae60");
        p.setPen(QPen(liveColor, 1.5, Qt::DashLine));
        p.drawLine(QPointF(curPt.x(), area.top()), QPointF(curPt.x(), area.bottom()));

        // Current value dot
        p.setPen(QPen(Qt::white, 2.0));
        p.setBrush(liveColor);
        p.drawEllipse(curPt, 5.0, 5.0);

        // Current badge at top
        QString liveText = QString("%1°C (%2%)").arg(m_currentTemp, 0, 'f', 1).arg(curPct, 0, 'f', 0);
        QRectF badgeRect(curPt.x() - 36, area.top() - 20, 72, 16);
        p.setPen(Qt::NoPen);
        p.setBrush(liveColor);
        p.drawRoundedRect(badgeRect, 3, 3);
        p.setPen(Qt::white);
        QFont bFont = font();
        bFont.setPointSize(7);
        bFont.setBold(true);
        p.setFont(bFont);
        p.drawText(badgeRect, Qt::AlignCenter, liveText);
    }

    // Draw control points
    for (int i = 0; i < m_points.size(); ++i) {
        QPointF pt = toCanvas(m_points[i].first, m_points[i].second);
        bool isHovered = (i == m_hoveredIndex || i == m_draggedIndex);

        double radius = isHovered ? 6.5 : 4.5;
        p.setPen(QPen(accentColor, 2.0));
        p.setBrush(isHovered ? accentColor : pal.color(QPalette::Base));
        p.drawEllipse(pt, radius, radius);
    }
}

void CurveEditor::mousePressEvent(QMouseEvent *event)
{
    if (!m_editable) return;
    if (event->button() == Qt::LeftButton) {
        int idx = findPointNear(event->position());
        if (idx >= 0) {
            m_draggedIndex = idx;
            m_hoveredIndex = idx;
            update();
        }
    }
}

void CurveEditor::mouseMoveEvent(QMouseEvent *event)
{
    if (!m_editable) return;

    if (m_draggedIndex >= 0 && m_draggedIndex < m_points.size()) {
        CurvePoint newPt = fromCanvas(event->position());

        // Keep points strictly sorted horizontally
        double minT = (m_draggedIndex > 0) ? m_points[m_draggedIndex - 1].first + 1.0 : m_minTemp;
        double maxT = (m_draggedIndex < m_points.size() - 1) ? m_points[m_draggedIndex + 1].first - 1.0 : m_maxTemp;

        // Monotonic constraint: higher temp CANNOT have lower fan speed!
        double minP = (m_draggedIndex > 0) ? m_points[m_draggedIndex - 1].second : m_minPct;
        double maxP = (m_draggedIndex < m_points.size() - 1) ? m_points[m_draggedIndex + 1].second : m_maxPct;

        newPt.first = std::clamp(newPt.first, minT, maxT);
        newPt.second = std::clamp(newPt.second, minP, maxP);

        m_points[m_draggedIndex] = newPt;

        QString pctText = (newPt.second == 0.0) ? "0% (EC Mode)" : QString("%1%").arg(newPt.second, 0, 'f', 0);
        QToolTip::showText(event->globalPosition().toPoint(),
                           QString("%1 °C → %2").arg(newPt.first, 0, 'f', 0).arg(pctText), this);

        emit curveChanged(m_points);
        update();
        return;
    }

    int idx = findPointNear(event->position());
    if (idx != m_hoveredIndex) {
        m_hoveredIndex = idx;
        if (idx >= 0) {
            setCursor(Qt::SizeAllCursor);
            double pct = m_points[idx].second;
            QString pctText = (pct == 0.0) ? "0% (EC Mode)" : QString("%1%").arg(pct, 0, 'f', 0);
            QToolTip::showText(event->globalPosition().toPoint(),
                               QString("%1 °C → %2").arg(m_points[idx].first, 0, 'f', 0).arg(pctText), this);
        } else {
            setCursor(Qt::ArrowCursor);
            QToolTip::hideText();
        }
        update();
    }
}

void CurveEditor::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && m_draggedIndex >= 0) {
        m_draggedIndex = -1;
        emit curveChanged(m_points);
        update();
    }
}

void CurveEditor::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (!m_editable) return;
    if (event->button() == Qt::LeftButton) {
        CurvePoint newPt = fromCanvas(event->position());
        int insertIdx = 0;
        while (insertIdx < m_points.size() && m_points[insertIdx].first < newPt.first) {
            insertIdx++;
        }

        // Enforce monotonic non-decreasing fan speed
        double minP = (insertIdx > 0) ? m_points[insertIdx - 1].second : m_minPct;
        double maxP = (insertIdx < m_points.size()) ? m_points[insertIdx].second : m_maxPct;
        newPt.second = std::clamp(newPt.second, minP, maxP);

        m_points.insert(insertIdx, newPt);
        emit curveChanged(m_points);
        update();
    }
}

void CurveEditor::contextMenuEvent(QContextMenuEvent *event)
{
    if (!m_editable) return;
    int idx = findPointNear(event->pos());
    if (idx >= 0) {
        QMenu menu(this);
        QAction *delAct = menu.addAction(tr("Delete Point"));
        if (m_points.size() <= 2) {
            delAct->setEnabled(false);
        }
        QAction *selected = menu.exec(event->globalPos());
        if (selected == delAct && m_points.size() > 2) {
            m_points.removeAt(idx);
            m_hoveredIndex = -1;
            m_draggedIndex = -1;
            emit curveChanged(m_points);
            update();
        }
    }
}
