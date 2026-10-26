#pragma once

#include <QList>
#include <QPair>
#include <QVector>
#include <QWidget>

class QBoxLayout;
class QTableWidget;
class QLabel;
class QPushButton;
class CurvePlot;

using CurvePoint = QPair<double, double>;
using CurvePoints = QList<CurvePoint>;

class CurveEditor : public QWidget
{
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
    void setTelemetryAvailable(bool available);

    double interpolatePercentAt(double temp) const;
    int selectedPointIndex() const;
    bool canUndo() const;
    bool canRedo() const;
    void undo();
    void redo();
    void addPoint();
    void removeSelectedPoint();

  signals:
    void curveChanged(const CurvePoints &points);
    void pointSelectionChanged(int index);
    void undoAvailabilityChanged(bool canUndo, bool canRedo);

  protected:
    void resizeEvent(QResizeEvent *event) override;

  private:
    friend class CurvePlot;
    void buildUi();
    void updateLayoutDirection();
    void syncTable();
    void syncSelection(int index);
    bool applyPointEdit(int index, const CurvePoint &point, bool recordUndo = true);
    bool validPointAt(int index, const CurvePoint &point) const;
    bool validCurve(const CurvePoints &points) const;
    void commitSnapshot(const CurvePoints &before);
    void restoreSnapshot(int index);
    void setValidationMessage(const QString &message);

    QRectF plotArea(const QSize &size) const;
    QPointF toCanvas(const QSize &size, double temp, double percent) const;
    CurvePoint fromCanvas(const QSize &size, const QPointF &pos) const;
    int findPointNear(const QSize &size, const QPointF &pos, double radius = 14.0) const;
    void paintPlot(QPainter &painter, const QSize &size);
    void plotMousePress(QMouseEvent *event);
    void plotMouseMove(QMouseEvent *event);
    void plotMouseRelease(QMouseEvent *event);
    void plotMouseDoubleClick(QMouseEvent *event);
    void plotContextMenu(QContextMenuEvent *event);

    CurvePoints m_points;
    double m_currentTemp{0.0};
    int m_draggedIndex{-1};
    int m_hoveredIndex{-1};
    int m_selectedIndex{-1};
    bool m_editable{true};
    bool m_telemetryAvailable{false};
    bool m_updatingTable{false};
    CurvePoints m_dragStart;
    QVector<CurvePoints> m_history;
    int m_historyIndex{-1};

    CurvePlot *m_plot{nullptr};
    QTableWidget *m_table{nullptr};
    QLabel *m_summary{nullptr};
    QLabel *m_validation{nullptr};
    QPushButton *m_addButton{nullptr};
    QPushButton *m_removeButton{nullptr};
    QBoxLayout *m_contentLayout{nullptr};

    const double m_minTemp{30.0};
    const double m_maxTemp{100.0};
    const double m_minPct{0.0};
    const double m_maxPct{100.0};
};
