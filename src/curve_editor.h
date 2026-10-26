#pragma once

#include <QWidget>
#include <QList>
#include <QPair>

using CurvePoint = QPair<double, double>;
using CurvePoints = QList<CurvePoint>;

class CurveEditor : public QWidget {
    Q_OBJECT
public:
    explicit CurveEditor(QWidget *parent = nullptr);
    ~CurveEditor() override = default;

    void setPoints(const CurvePoints &points);
    CurvePoints points() const;

    void setCurrentTemperature(double tempCelsius);
    double currentTemperature() const;

    void setEditable(bool editable);
    bool isEditable() const;

    double interpolatePercentAt(double temp) const;

signals:
    void curveChanged(const CurvePoints &points);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;

private:
    QRectF plotArea() const;
    QPointF toCanvas(double temp, double percent) const;
    CurvePoint fromCanvas(const QPointF &pos) const;
    int findPointNear(const QPointF &pos, double radius = 12.0) const;

    CurvePoints m_points;
    double m_currentTemp{0.0};
    int m_draggedIndex{-1};
    int m_hoveredIndex{-1};
    bool m_editable{true};

    const double m_minTemp{30.0};
    const double m_maxTemp{100.0};
    const double m_minPct{0.0};
    const double m_maxPct{100.0};
};
