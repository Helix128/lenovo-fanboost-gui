#include "curve_editor.h"

#include <QAccessible>
#include <QAction>
#include <QBoxLayout>
#include <QContextMenuEvent>
#include <QDoubleSpinBox>
#include <QHeaderView>
#include <QLabel>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QResizeEvent>
#include <QStyle>
#include <QStyleOption>
#include <QStyledItemDelegate>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QToolTip>
#include <algorithm>
#include <cmath>

class CurvePlot final : public QWidget
{
  public:
    explicit CurvePlot(CurveEditor *editor) : QWidget(editor), m_editor(editor)
    {
        setObjectName("curvePlot");
        setFocusPolicy(Qt::StrongFocus);
        setAccessibleName(tr("Fan curve graph"));
        setAccessibleDescription(tr("Graph of fan level by CPU temperature. Select a point in the table to "
                                    "locate it. Edit values in the table for precise keyboard access."));
        setMouseTracking(true);
        setMinimumSize(fontMetrics().horizontalAdvance(QStringLiteral("100")) * 8 + 80,
                       std::max(220, fontMetrics().height() * 16));
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    }

  protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        m_editor->paintPlot(painter, size());
    }
    void mousePressEvent(QMouseEvent *event) override { m_editor->plotMousePress(event); }
    void mouseMoveEvent(QMouseEvent *event) override { m_editor->plotMouseMove(event); }
    void mouseReleaseEvent(QMouseEvent *event) override { m_editor->plotMouseRelease(event); }
    void mouseDoubleClickEvent(QMouseEvent *event) override { m_editor->plotMouseDoubleClick(event); }
    void contextMenuEvent(QContextMenuEvent *event) override { m_editor->plotContextMenu(event); }

  private:
    CurveEditor *m_editor;
};

class CurveValueDelegate final : public QStyledItemDelegate
{
  public:
    CurveValueDelegate(double minimum, double maximum, QObject *parent)
        : QStyledItemDelegate(parent), m_minimum(minimum), m_maximum(maximum)
    {
    }

    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &, const QModelIndex &) const override
    {
        auto *editor = new QDoubleSpinBox(parent);
        editor->setRange(m_minimum, m_maximum);
        editor->setDecimals(6);
        editor->setStepType(QAbstractSpinBox::AdaptiveDecimalStepType);
        editor->setKeyboardTracking(false);
        editor->setButtonSymbols(QAbstractSpinBox::UpDownArrows);
        editor->setAccessibleName(m_minimum == 30 ? tr("Temperature in degrees Celsius")
                                                  : tr("Fan level in percent"));
        return editor;
    }

    void setEditorData(QWidget *editor, const QModelIndex &index) const override
    {
        static_cast<QDoubleSpinBox *>(editor)->setValue(index.data(Qt::EditRole).toDouble());
    }

    void setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const override
    {
        model->setData(index, static_cast<QDoubleSpinBox *>(editor)->value(), Qt::EditRole);
    }

  private:
    double m_minimum;
    double m_maximum;
};

CurveEditor::CurveEditor(QWidget *parent) : QWidget(parent)
{
    setObjectName("curveEditor");
    setAccessibleName(tr("Fan curve editor"));
    setAccessibleDescription(tr("Edit fan curve points by temperature and fan level. The graph and point "
                                "table share the same selection."));
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setMinimumSize(0, 0);
    buildUi();
}

void CurveEditor::buildUi()
{
    m_contentLayout = new QBoxLayout(QBoxLayout::LeftToRight, this);
    m_contentLayout->setContentsMargins(0, 0, 0, 0);
    m_contentLayout->setSpacing(12);

    auto *graphPanel = new QWidget(this);
    auto *graphLayout = new QVBoxLayout(graphPanel);
    graphLayout->setContentsMargins(0, 0, 0, 0);
    graphLayout->setSpacing(8);
    m_plot = new CurvePlot(this);
    graphLayout->addWidget(m_plot, 1);
    m_summary = new QLabel(tr("Select a point to view its values."), graphPanel);
    m_summary->setObjectName("selectedPointSummary");
    m_summary->setAccessibleName(tr("Selected curve point"));
    graphLayout->addWidget(m_summary);

    auto *tablePanel = new QWidget(this);
    tablePanel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    auto *tableLayout = new QVBoxLayout(tablePanel);
    tableLayout->setContentsMargins(0, 0, 0, 0);
    tableLayout->setSpacing(8);
    m_table = new QTableWidget(tablePanel);
    m_table->setObjectName("curvePointTable");
    m_table->setAccessibleName(tr("Fan curve points"));
    m_table->setAccessibleDescription(
        tr("Temperature and fan level for each curve point. Edit either numeric value. Temperatures must "
           "increase and fan levels may not decrease."));
    m_table->setColumnCount(2);
    m_table->setHorizontalHeaderLabels({tr("Temperature (°C)"), tr("Fan level (%)")});
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_table->horizontalHeader()->setMinimumSectionSize(
        std::max(fontMetrics().horizontalAdvance(tr("Temperature (°C)")),
                 fontMetrics().horizontalAdvance(tr("Fan level (%)"))) +
        24);
    m_table->verticalHeader()->setVisible(false);
    m_table->verticalHeader()->setDefaultSectionSize(fontMetrics().height() + 16);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setTabKeyNavigation(false);
    m_table->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed |
                             QAbstractItemView::SelectedClicked);
    m_table->setItemDelegateForColumn(0, new CurveValueDelegate(30, 100, m_table));
    m_table->setItemDelegateForColumn(1, new CurveValueDelegate(0, 100, m_table));
    tableLayout->addWidget(m_table, 1);

    auto *buttonRow = new QHBoxLayout;
    m_addButton = new QPushButton(tr("Add point"), tablePanel);
    m_removeButton = new QPushButton(tr("Remove point"), tablePanel);
    m_addButton->setAccessibleName(tr("Add curve point"));
    m_removeButton->setAccessibleName(tr("Remove selected curve point"));
    buttonRow->addWidget(m_addButton);
    buttonRow->addWidget(m_removeButton);
    tableLayout->addLayout(buttonRow);

    m_validation = new QLabel(tablePanel);
    m_validation->setObjectName("curveValidationMessage");
    m_validation->setWordWrap(true);
    m_validation->setAccessibleName(tr("Curve validation"));
    tableLayout->addWidget(m_validation);

    m_contentLayout->addWidget(graphPanel, 3);
    m_contentLayout->addWidget(tablePanel, 2);
    connect(m_addButton, &QPushButton::clicked, this, &CurveEditor::addPoint);
    connect(m_removeButton, &QPushButton::clicked, this, &CurveEditor::removeSelectedPoint);
    connect(m_table, &QTableWidget::currentCellChanged, this,
            [this](int row, int, int, int) { syncSelection(row); });
    connect(m_table, &QTableWidget::itemChanged, this, [this](QTableWidgetItem *item) {
        if (m_updatingTable || item->row() < 0 || item->row() >= m_points.size())
            return;
        const int row = item->row();
        const CurvePoint oldPoint = m_points[row];
        CurvePoint candidate = oldPoint;
        bool parsed = false;
        const double value = item->text().toDouble(&parsed);
        if (parsed && item->column() == 0)
            candidate.first = value;
        else if (parsed)
            candidate.second = value;
        if (!parsed || !applyPointEdit(row, candidate)) {
            syncTable();
            setValidationMessage(tr("Invalid point. Temperatures must be unique and increasing from 30 to "
                                    "100 °C; fan levels must be nondecreasing from 0 to 100%."));
        } else {
            setValidationMessage(QString());
        }
    });

    auto *deleteAction = new QAction(this);
    deleteAction->setShortcut(QKeySequence::Delete);
    deleteAction->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    connect(deleteAction, &QAction::triggered, this, &CurveEditor::removeSelectedPoint);
    addAction(deleteAction);
    auto *insertAction = new QAction(this);
    insertAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_I));
    insertAction->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    connect(insertAction, &QAction::triggered, this, &CurveEditor::addPoint);
    addAction(insertAction);
    auto *undoAction = new QAction(this);
    undoAction->setShortcut(QKeySequence::Undo);
    undoAction->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    connect(undoAction, &QAction::triggered, this, &CurveEditor::undo);
    addAction(undoAction);
    auto *redoAction = new QAction(this);
    redoAction->setShortcut(QKeySequence::Redo);
    redoAction->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    connect(redoAction, &QAction::triggered, this, &CurveEditor::redo);
    addAction(redoAction);

    m_history.append(m_points);
    m_historyIndex = 0;
    updateLayoutDirection();
}

void CurveEditor::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    updateLayoutDirection();
}

void CurveEditor::updateLayoutDirection()
{
    const auto direction = width() < std::max(720, fontMetrics().height() * 42) ? QBoxLayout::TopToBottom
                                                                                : QBoxLayout::LeftToRight;
    if (m_contentLayout->direction() != direction)
        m_contentLayout->setDirection(direction);
}

void CurveEditor::setPoints(const CurvePoints &points)
{
    if (!validCurve(points)) {
        setValidationMessage(
            tr("Curve was not loaded: provide 2–64 finite points with unique increasing temperatures from 30 "
               "to 100 °C and nondecreasing fan levels from 0 to 100%."));
        return;
    }
    m_points = points;
    m_selectedIndex =
        m_points.isEmpty() ? -1 : std::clamp(m_selectedIndex, 0, static_cast<int>(m_points.size()) - 1);
    m_history = {m_points};
    m_historyIndex = 0;
    syncTable();
    setValidationMessage(QString());
    emit undoAvailabilityChanged(false, false);
    m_plot->update();
}

CurvePoints CurveEditor::points() const { return m_points; }
void CurveEditor::setCurrentTemperature(double value)
{
    m_currentTemp = value;
    m_plot->update();
}
double CurveEditor::currentTemperature() const { return m_currentTemp; }

void CurveEditor::setEditable(bool editable)
{
    m_editable = editable;
    m_table->setEditTriggers(editable ? QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed |
                                            QAbstractItemView::SelectedClicked
                                      : QAbstractItemView::NoEditTriggers);
    m_addButton->setEnabled(editable);
    m_removeButton->setEnabled(editable && m_points.size() > 2 && m_selectedIndex >= 0);
    m_plot->setAccessibleDescription(
        editable ? tr("Graph of fan level by CPU temperature. Select a point in the table to locate it. Edit "
                      "values in the table for precise keyboard access.")
                 : tr("Read-only graph of fan level by CPU temperature. The point table remains available "
                      "for reading."));
    m_plot->update();
    emit undoAvailabilityChanged(canUndo(), canRedo());
}
bool CurveEditor::isEditable() const { return m_editable; }
void CurveEditor::setTelemetryAvailable(bool available)
{
    m_telemetryAvailable = available;
    m_plot->update();
}
int CurveEditor::selectedPointIndex() const { return m_selectedIndex; }
bool CurveEditor::canUndo() const { return m_editable && m_historyIndex > 0; }
bool CurveEditor::canRedo() const
{
    return m_editable && m_historyIndex >= 0 && m_historyIndex + 1 < m_history.size();
}

void CurveEditor::undo()
{
    if (!canUndo())
        return;
    restoreSnapshot(--m_historyIndex);
}
void CurveEditor::redo()
{
    if (!canRedo())
        return;
    restoreSnapshot(++m_historyIndex);
}

void CurveEditor::restoreSnapshot(int index)
{
    if (index < 0 || index >= m_history.size())
        return;
    m_points = m_history[index];
    m_selectedIndex =
        m_points.isEmpty() ? -1 : std::clamp(m_selectedIndex, 0, static_cast<int>(m_points.size()) - 1);
    syncTable();
    m_plot->update();
    emit curveChanged(m_points);
    emit undoAvailabilityChanged(canUndo(), canRedo());
}

void CurveEditor::commitSnapshot(const CurvePoints &before)
{
    if (before == m_points)
        return;
    while (m_history.size() > m_historyIndex + 1)
        m_history.removeLast();
    m_history.append(m_points);
    if (m_history.size() > 100)
        m_history.removeFirst();
    m_historyIndex = m_history.size() - 1;
    emit undoAvailabilityChanged(canUndo(), canRedo());
}

void CurveEditor::syncTable()
{
    m_updatingTable = true;
    m_table->setRowCount(m_points.size());
    for (int row = 0; row < m_points.size(); ++row) {
        for (int col = 0; col < 2; ++col) {
            auto *item = m_table->item(row, col);
            if (!item) {
                item = new QTableWidgetItem;
                item->setTextAlignment(Qt::AlignCenter);
                m_table->setItem(row, col, item);
            }
            const double value = col == 0 ? m_points[row].first : m_points[row].second;
            const QString text = QString::number(value, 'g', 12);
            item->setText(text);
            item->setData(Qt::AccessibleTextRole,
                          col == 0 ? tr("Point %1 temperature, %2 degrees Celsius").arg(row + 1).arg(text)
                                   : tr("Point %1 fan level, %2 percent").arg(row + 1).arg(text));
        }
    }
    if (m_selectedIndex >= 0 && m_selectedIndex < m_points.size()) {
        m_table->selectRow(m_selectedIndex);
        m_table->setCurrentCell(m_selectedIndex, 1);
    } else {
        m_table->clearSelection();
    }
    m_updatingTable = false;
    syncSelection(m_selectedIndex);
}

void CurveEditor::syncSelection(int index)
{
    if (index < -1 || index >= m_points.size())
        return;
    if (index >= 0 && m_table->currentRow() != index) {
        m_table->selectRow(index);
        m_table->setCurrentCell(index, 1);
    }
    if (m_selectedIndex != index) {
        m_selectedIndex = index;
        emit pointSelectionChanged(index);
    }
    m_removeButton->setEnabled(m_editable && m_points.size() > 2 && index >= 0);
    if (index >= 0) {
        const auto point = m_points[index];
        m_summary->setText(
            tr("Selected point: %1 °C · %2%")
                .arg(QString::number(point.first, 'g', 12), QString::number(point.second, 'g', 12)));
        m_plot->setAccessibleDescription(tr("Fan curve graph. Selected point: %1 degrees Celsius, %2 "
                                            "percent. Use the point table to edit values.")
                                             .arg(qRound(point.first))
                                             .arg(qRound(point.second)));
    } else {
        m_summary->setText(tr("Select a point to view its values."));
    }
    m_plot->update();
}

bool CurveEditor::validPointAt(int index, const CurvePoint &point) const
{
    if (index < 0 || index >= m_points.size())
        return false;
    if (!std::isfinite(point.first) || !std::isfinite(point.second))
        return false;
    if (point.first < m_minTemp || point.first > m_maxTemp || point.second < m_minPct ||
        point.second > m_maxPct)
        return false;
    if (index > 0 && (point.first <= m_points[index - 1].first || point.second < m_points[index - 1].second))
        return false;
    if (index + 1 < m_points.size() &&
        (point.first >= m_points[index + 1].first || point.second > m_points[index + 1].second))
        return false;
    return true;
}

bool CurveEditor::validCurve(const CurvePoints &points) const
{
    if (points.size() < 2 || points.size() > 64)
        return false;
    for (int i = 0; i < points.size(); ++i) {
        const auto p = points[i];
        if (!std::isfinite(p.first) || !std::isfinite(p.second))
            return false;
        if (p.first < m_minTemp || p.first > m_maxTemp || p.second < m_minPct || p.second > m_maxPct)
            return false;
        if (i && (p.first <= points[i - 1].first || p.second < points[i - 1].second))
            return false;
    }
    return true;
}

bool CurveEditor::applyPointEdit(int index, const CurvePoint &point, bool recordUndo)
{
    if (!m_editable || !validPointAt(index, point))
        return false;
    const CurvePoints before = m_points;
    m_points[index] = point;
    if (recordUndo)
        commitSnapshot(before);
    syncTable();
    m_plot->update();
    emit curveChanged(m_points);
    return true;
}

void CurveEditor::addPoint()
{
    if (!m_editable)
        return;
    if (m_points.size() < 2 || m_points.size() >= 64) {
        setValidationMessage(tr("A curve must contain 2–64 points."));
        return;
    }
    int left = 0;
    int right = 1;
    if (m_selectedIndex >= 0 && m_selectedIndex + 1 < m_points.size()) {
        left = m_selectedIndex;
        right = left + 1;
    } else if (m_selectedIndex > 0) {
        right = m_selectedIndex;
        left = right - 1;
    } else {
        double widestGap = 0;
        for (int i = 0; i + 1 < m_points.size(); ++i) {
            const double gap = m_points[i + 1].first - m_points[i].first;
            if (gap > widestGap) {
                widestGap = gap;
                left = i;
                right = i + 1;
            }
        }
    }
    const int t = qRound((m_points[left].first + m_points[right].first) / 2.0);
    if (t <= m_points[left].first || t >= m_points[right].first) {
        setValidationMessage(tr("No unused whole-degree temperature remains between the selected points."));
        return;
    }
    CurvePoints before = m_points;
    const double pct = std::round(interpolatePercentAt(t));
    m_points.insert(right, CurvePoint(t, pct));
    m_selectedIndex = right;
    commitSnapshot(before);
    syncTable();
    setValidationMessage(QString());
    m_plot->update();
    emit curveChanged(m_points);
    m_table->setFocus();
    m_table->editItem(m_table->item(right, 0));
}

void CurveEditor::removeSelectedPoint()
{
    if (!m_editable || m_points.size() <= 2 || m_selectedIndex < 0 || m_selectedIndex >= m_points.size())
        return;
    const CurvePoints before = m_points;
    const int removed = m_selectedIndex;
    m_points.removeAt(removed);
    m_selectedIndex = std::min(removed, static_cast<int>(m_points.size()) - 1);
    commitSnapshot(before);
    syncTable();
    setValidationMessage(QString());
    m_plot->update();
    emit curveChanged(m_points);
    m_table->setFocus();
}

void CurveEditor::setValidationMessage(const QString &message)
{
    m_validation->setText(message);
    m_validation->setAccessibleDescription(message);
    if (!message.isEmpty()) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
        QAccessibleAnnouncementEvent event(m_validation, message);
#else
        QAccessibleEvent event(m_validation, QAccessible::Alert);
#endif
        QAccessible::updateAccessibility(&event);
    }
}

double CurveEditor::interpolatePercentAt(double temp) const
{
    if (m_points.isEmpty())
        return 0.0;
    if (temp <= m_points.first().first)
        return m_points.first().second;
    if (temp >= m_points.last().first)
        return m_points.last().second;
    for (int i = 0; i < m_points.size() - 1; ++i) {
        const double t0 = m_points[i].first, p0 = m_points[i].second;
        const double t1 = m_points[i + 1].first, p1 = m_points[i + 1].second;
        if (temp >= t0 && temp <= t1)
            return p0 + ((temp - t0) / (t1 - t0)) * (p1 - p0);
    }
    return 0.0;
}

QRectF CurveEditor::plotArea(const QSize &size) const
{
    const QFontMetrics fm(font());
    const double left = fm.horizontalAdvance(QStringLiteral("100%")) + 22.0;
    const double top = fm.height() * 2.1;
    const double right = 14.0;
    const double bottom = fm.height() * 3.4;
    return QRectF(left, top, std::max(1.0, size.width() - left - right),
                  std::max(1.0, size.height() - top - bottom));
}

QPointF CurveEditor::toCanvas(const QSize &size, double temp, double percent) const
{
    const QRectF area = plotArea(size);
    return {area.left() + (temp - m_minTemp) / (m_maxTemp - m_minTemp) * area.width(),
            area.bottom() - (percent - m_minPct) / (m_maxPct - m_minPct) * area.height()};
}

CurvePoint CurveEditor::fromCanvas(const QSize &size, const QPointF &pos) const
{
    const QRectF area = plotArea(size);
    const double temp =
        std::round(m_minTemp + (pos.x() - area.left()) / area.width() * (m_maxTemp - m_minTemp));
    const double pct =
        std::round(m_minPct + (area.bottom() - pos.y()) / area.height() * (m_maxPct - m_minPct));
    return {std::clamp(temp, m_minTemp, m_maxTemp), std::clamp(pct, m_minPct, m_maxPct)};
}

int CurveEditor::findPointNear(const QSize &size, const QPointF &pos, double radius) const
{
    for (int i = 0; i < m_points.size(); ++i) {
        const QPointF point = toCanvas(size, m_points[i].first, m_points[i].second);
        if (std::hypot(point.x() - pos.x(), point.y() - pos.y()) <= radius)
            return i;
    }
    return -1;
}

void CurveEditor::paintPlot(QPainter &p, const QSize &size)
{
    p.setRenderHint(QPainter::Antialiasing, true);
    const QRectF area = plotArea(size);
    const QPalette pal = m_plot->palette();
    const QColor grid = pal.color(QPalette::Mid);
    p.fillRect(area, pal.color(QPalette::Base));
    p.setFont(m_plot->font());
    p.setPen(grid);
    for (int pct = 0; pct <= 100; pct += 20) {
        const QPointF point = toCanvas(size, m_minTemp, pct);
        p.setPen(grid);
        p.drawLine(QPointF(area.left(), point.y()), QPointF(area.right(), point.y()));
        p.setPen(pal.color(QPalette::Text));
        p.drawText(
            QRectF(0, point.y() - fontMetrics().height() / 2.0, area.left() - 7, fontMetrics().height()),
            Qt::AlignRight | Qt::AlignVCenter, pct == 0 ? tr("0% (EC)") : QString::number(pct) + '%');
    }
    for (int temp = 30; temp <= 100; temp += 10) {
        const QPointF point = toCanvas(size, temp, m_minPct);
        p.setPen(grid);
        p.drawLine(QPointF(point.x(), area.top()), QPointF(point.x(), area.bottom()));
        p.setPen(pal.color(QPalette::Text));
        p.drawText(QRectF(point.x() - 24, area.bottom() + 4, 48, fontMetrics().height()), Qt::AlignHCenter,
                   QString::number(temp));
    }
    p.setPen(pal.color(QPalette::WindowText));
    p.drawText(
        QRectF(area.left(), area.bottom() + fontMetrics().height(), area.width(), fontMetrics().height()),
        Qt::AlignHCenter, tr("CPU temperature (°C)"));
    p.save();
    p.translate(12, area.center().y());
    p.rotate(-90);
    p.drawText(QRectF(-area.height() / 2.0, -fontMetrics().height(), area.height(), fontMetrics().height()),
               Qt::AlignHCenter, tr("Fan level (%)"));
    p.restore();
    p.setPen(QPen(pal.color(QPalette::Mid), 1.0));
    p.setBrush(Qt::NoBrush);
    p.drawRect(area);

    if (m_points.size() >= 2) {
        QPainterPath curve;
        curve.moveTo(toCanvas(size, m_minTemp, m_points.first().second));
        for (int i = 0; i < m_points.size(); ++i)
            curve.lineTo(toCanvas(size, m_points[i].first, m_points[i].second));
        curve.lineTo(toCanvas(size, m_maxTemp, m_points.last().second));
        p.setPen(QPen(pal.color(QPalette::Link), 2.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.drawPath(curve);
    }
    if (m_telemetryAvailable && m_currentTemp >= m_minTemp && m_currentTemp <= m_maxTemp) {
        const QPointF live = toCanvas(size, m_currentTemp, interpolatePercentAt(m_currentTemp));
        p.setPen(QPen(pal.color(QPalette::PlaceholderText), 1.5, Qt::DashLine));
        p.drawLine(QPointF(live.x(), area.top()), QPointF(live.x(), area.bottom()));
        p.setBrush(pal.color(QPalette::Link));
        p.setPen(QPen(pal.color(QPalette::Base), 2));
        p.drawEllipse(live, 5, 5);
        const QString liveText = tr("Current: %1 °C").arg(m_currentTemp, 0, 'f', 1);
        p.setPen(pal.color(QPalette::WindowText));
        const double labelWidth = fontMetrics().horizontalAdvance(liveText) + 12;
        const double labelLeft =
            std::clamp(live.x() - labelWidth / 2, 0.0, std::max(0.0, size.width() - labelWidth));
        p.drawText(QRectF(labelLeft, 0, labelWidth, fontMetrics().height() * 1.5), Qt::AlignCenter, liveText);
    }
    for (int i = 0; i < m_points.size(); ++i) {
        const QPointF point = toCanvas(size, m_points[i].first, m_points[i].second);
        const bool selected = i == m_selectedIndex;
        p.setPen(QPen(pal.color(QPalette::Link), selected ? 2.5 : 1.5));
        p.setBrush(selected ? pal.color(QPalette::Link) : pal.color(QPalette::Base));
        p.drawEllipse(point, selected ? 7.0 : 5.0, selected ? 7.0 : 5.0);
    }
    if (m_plot->hasFocus()) {
        QStyleOptionFocusRect option;
        option.initFrom(m_plot);
        option.rect = m_plot->rect().adjusted(1, 1, -2, -2);
        m_plot->style()->drawPrimitive(QStyle::PE_FrameFocusRect, &option, &p, m_plot);
    }
}

void CurveEditor::plotMousePress(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton || !m_editable)
        return;
    const int index = findPointNear(m_plot->size(), event->position());
    if (index >= 0) {
        m_plot->setFocus(Qt::MouseFocusReason);
        m_draggedIndex = index;
        m_dragStart = m_points;
        syncSelection(index);
        m_hoveredIndex = index;
        m_plot->update();
    }
}

void CurveEditor::plotMouseMove(QMouseEvent *event)
{
    if (!m_editable)
        return;
    if (m_draggedIndex >= 0 && m_draggedIndex < m_points.size()) {
        const CurvePoint point = fromCanvas(m_plot->size(), event->position());
        if (validPointAt(m_draggedIndex, point)) {
            m_points[m_draggedIndex] = point;
            syncTable();
            m_plot->update();
            emit curveChanged(m_points);
        }
        const QString level =
            point.second == 0 ? tr("0% (EC automatic)") : QString::number(qRound(point.second)) + '%';
        QToolTip::showText(event->globalPosition().toPoint(),
                           tr("%1 °C → %2").arg(qRound(point.first)).arg(level), m_plot);
        return;
    }
    const int index = findPointNear(m_plot->size(), event->position());
    if (index != m_hoveredIndex) {
        m_hoveredIndex = index;
        m_plot->setCursor(index >= 0 ? Qt::SizeAllCursor : Qt::ArrowCursor);
        m_plot->update();
    }
}

void CurveEditor::plotMouseRelease(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && m_draggedIndex >= 0) {
        commitSnapshot(m_dragStart);
        m_draggedIndex = -1;
        m_dragStart.clear();
        m_plot->update();
    }
}

void CurveEditor::plotMouseDoubleClick(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton || !m_editable)
        return;
    if (m_points.size() >= 64) {
        setValidationMessage(tr("A curve cannot contain more than 64 points."));
        return;
    }
    const CurvePoint candidate = fromCanvas(m_plot->size(), event->position());
    int index = 0;
    while (index < m_points.size() && m_points[index].first < candidate.first)
        ++index;
    if ((index > 0 && candidate.first <= m_points[index - 1].first) ||
        (index < m_points.size() && candidate.first >= m_points[index].first)) {
        setValidationMessage(tr("A point already uses that temperature. Choose an unused temperature."));
        return;
    }
    const double lowerPct = index ? m_points[index - 1].second : m_minPct;
    const double upperPct = index < m_points.size() ? m_points[index].second : m_maxPct;
    if (candidate.second < lowerPct || candidate.second > upperPct) {
        setValidationMessage(tr("Fan level must stay between the neighboring point levels."));
        return;
    }
    const CurvePoints before = m_points;
    m_points.insert(index, candidate);
    m_selectedIndex = index;
    commitSnapshot(before);
    syncTable();
    setValidationMessage(QString());
    m_plot->update();
    emit curveChanged(m_points);
}

void CurveEditor::plotContextMenu(QContextMenuEvent *event)
{
    if (!m_editable)
        return;
    const int index = findPointNear(m_plot->size(), event->pos());
    if (index < 0)
        return;
    syncSelection(index);
    QMenu menu(this);
    QAction *remove = menu.addAction(tr("Remove selected point"));
    remove->setEnabled(m_points.size() > 2);
    if (menu.exec(event->globalPos()) == remove)
        removeSelectedPoint();
}
