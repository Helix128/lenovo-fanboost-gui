#include "curve_editor.h"

#include <QDoubleSpinBox>
#include <QTableWidget>
#include <QtTest>
#include <limits>

class CurveEditorTest : public QObject
{
    Q_OBJECT
  private slots:
    void tableEditsAndSelectionStaySynchronized()
    {
        CurveEditor editor;
        QVERIFY(!editor.accessibleName().isEmpty());
        editor.resize(1100, 700);
        editor.setPoints({{40, 0}, {50, 20}, {60, 40}, {90, 100}});
        editor.show();
        auto *table = editor.findChild<QTableWidget *>("curvePointTable");
        QVERIFY(table);
        table->setCurrentCell(2, 1);
        QCOMPARE(editor.selectedPointIndex(), 2);
        table->item(2, 1)->setText("45");
        QCOMPARE(editor.points()[2].second, 45.0);
        QCOMPARE(editor.interpolatePercentAt(60), 45.0);
        QCOMPARE(table->item(2, 1)->text(), QString("45"));

        auto *plot = editor.findChild<QWidget *>("curvePlot");
        QVERIFY(plot);
        const auto fm = plot->fontMetrics();
        const double left = fm.horizontalAdvance(QStringLiteral("100%")) + 22.0;
        const double top = fm.height() * 2.1;
        const double x = left + (60.0 - 30.0) / 70.0 * (plot->width() - left - 14.0);
        const double y = top + (100.0 - 45.0) / 100.0 * (plot->height() - top - fm.height() * 3.4);
        QTest::mouseClick(plot, Qt::LeftButton, Qt::NoModifier, QPoint(qRound(x), qRound(y)));
        QCOMPARE(editor.selectedPointIndex(), 2);
    }

    void invalidTableEditsAreRejected()
    {
        CurveEditor editor;
        editor.setPoints({{40, 0}, {50, 20}, {60, 40}, {90, 100}});
        auto *table = editor.findChild<QTableWidget *>("curvePointTable");
        QVERIFY(table);
        const auto original = editor.points();
        struct Edit {
            int row;
            int column;
            QString value;
        };
        const QList<Edit> edits = {{1, 0, "40"}, {2, 1, "10"}, {2, 1, "101"}, {2, 0, "101"}};
        for (const auto &edit : edits) {
            table->item(edit.row, edit.column)->setText(edit.value);
            QCOMPARE(editor.points(), original);
        }
    }

    void addRemoveAndUndoRedo()
    {
        CurveEditor editor;
        editor.setPoints({{40, 0}, {60, 40}, {90, 100}});
        editor.addPoint();
        QCOMPARE(editor.points().size(), 4);
        QVERIFY(editor.canUndo());
        editor.undo();
        QCOMPARE(editor.points().size(), 3);
        QVERIFY(editor.canRedo());
        editor.redo();
        QCOMPARE(editor.points().size(), 4);
        editor.removeSelectedPoint();
        QCOMPARE(editor.points().size(), 3);
        editor.setPoints({{40, 0}, {90, 100}});
        editor.removeSelectedPoint();
        QCOMPARE(editor.points().size(), 2);
    }

    void fractionalPointsAndInvalidLoadsArePreserved()
    {
        CurveEditor editor;
        const CurvePoints initial = {{30.25, 0.5}, {80.75, 25.75}};
        editor.setPoints(initial);
        auto *table = editor.findChild<QTableWidget *>("curvePointTable");
        QVERIFY(table);
        QCOMPARE(table->item(0, 0)->text(), QString("30.25"));
        QCOMPARE(table->item(0, 1)->text(), QString("0.5"));
        table->item(1, 1)->setText("25.875");
        QCOMPARE(editor.points()[1].second, 25.875);
        editor.show();
        table->setCurrentCell(1, 1);
        table->editItem(table->item(1, 1));
        auto *spin = table->findChild<QDoubleSpinBox *>();
        QVERIFY(spin);
        QCOMPARE(spin->decimals(), 6);
        spin->setValue(26.125);
        QCOMPARE(spin->value(), 26.125);

        const CurvePoints edited = editor.points();
        editor.setPoints({{30.25, 0.5}, {30.25, 25.875}});
        QCOMPARE(editor.points(), edited);
        editor.setPoints({{30.25, 0.5}, {std::numeric_limits<double>::infinity(), 25.875}});
        QCOMPARE(editor.points(), edited);
        editor.setPoints({{30.25, 0.5}, {std::numeric_limits<double>::quiet_NaN(), 25.875}});
        QCOMPARE(editor.points(), edited);
        editor.undo();
        QCOMPARE(editor.points(), initial);
    }

    void curveLimitIs64()
    {
        CurveEditor editor;
        CurvePoints points;
        for (int i = 0; i < 64; ++i)
            points.append({30.0 + i, i * 100.0 / 63.0});
        editor.setPoints(points);
        editor.addPoint();
        QCOMPARE(editor.points().size(), 64);
        points.append({95, 100});
        editor.setPoints(points);
        QCOMPARE(editor.points().size(), 64);
    }
};

QTEST_MAIN(CurveEditorTest)
#include "test_curve_editor.moc"
