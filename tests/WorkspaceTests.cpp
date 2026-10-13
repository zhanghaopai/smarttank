#include <QtTest>
#include <QTreeWidget>
#include <QLineEdit>
#include <QLabel>
#include <QAction>
#include <QDockWidget>
#include <QTemporaryDir>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QImage>
#include <BRepBndLib.hxx>
#include <Bnd_Box.hxx>
#include <BRepCheck_Analyzer.hxx>
#include "ui/MainWindow.h"
#include "view/OcctView.h"
using namespace smarttank;

class WorkspaceTests : public QObject {
    Q_OBJECT
private slots:
    void componentParametersAndViewDoNotChangeInputs() {
        MainWindow window;
        window.show();
        auto* tree = window.findChild<QTreeWidget*>("componentTree");
        QVERIFY(tree);
        QCOMPARE(tree->topLevelItemCount(), 1);
        QCOMPARE(tree->topLevelItem(0)->childCount(), 3);
        QCOMPARE(window.dockWidgetArea(window.findChild<QDockWidget*>("parametersDock")), Qt::LeftDockWidgetArea);
        QCOMPARE(window.dockWidgetArea(window.findChild<QDockWidget*>("componentsDock")), Qt::RightDockWidgetArea);
        auto* diameter = window.findChild<QLineEdit*>("diameter_mm");
        QVERIFY(diameter && diameter->text().isEmpty());
        QTest::keyClicks(diameter, "2000");
        QTest::keyClick(diameter, Qt::Key_Return);
        QCOMPARE(window.project().parameters(Part::Shell).at("diameter_mm"), 2000.0);
        const auto revision = window.project().revision();
        tree->setCurrentItem(tree->topLevelItem(0)->child(2));
        QVERIFY(window.findChild<QLineEdit*>("rise_mm"));
        QVERIFY(!window.findChild<QLineEdit*>("diameter_mm"));
        QCOMPARE(window.project().revision(), revision);
        window.view()->fitAll();
        window.view()->highlightPart(Part::Roof);
        QCOMPARE(window.project().revision(), revision);
        tree->setCurrentItem(tree->topLevelItem(0)->child(1));
        QCOMPARE(window.findChild<QLineEdit*>("diameter_mm")->text(), QStringLiteral("2000"));
        diameter = window.findChild<QLineEdit*>("diameter_mm");
        diameter->clear();
        QTest::keyClick(diameter, Qt::Key_Return);
        QVERIFY(!window.project().parameters(Part::Shell).count("diameter_mm"));
        QVERIFY(window.project().revision() > revision);
    }
    void unavailableRulesNeverCreateResults() {
        MainWindow window;
        window.findChild<QAction*>("generateLayoutAction")->trigger();
        window.findChild<QAction*>("calculateAction")->trigger();
        QVERIFY(!window.project().hasCurrentLayout());
        QVERIFY(!window.project().hasCurrentChecks());
        QVERIFY(window.project().plates().empty());
        QVERIFY(window.project().checks().empty());
        QVERIFY(window.findChild<QLabel*>("basisStatus")->text().contains(QStringLiteral("未确定")));
    }
    void installedSurfacesAreIndependentOfManufacturingContours() {
        for (auto kind : {InstalledSurface::Kind::Plane, InstalledSurface::Kind::Cylinder, InstalledSurface::Kind::Cone}) {
            InstalledSurface definition;
            definition.kind = kind;
            definition.radius = 1000;
            definition.coneSemiAngle = -0.4;
            definition.parameterBoundary = kind == InstalledSurface::Kind::Plane
                ? std::vector<Point2>{{0,0},{1000,0},{1000,1000},{0,1000}}
                : std::vector<Point2>{{0,0},{0.5,0},{0.5,500},{0,500}};
            const auto shape = makeInstalledShape(definition);
            QVERIFY(!shape.IsNull());
            QVERIFY(BRepCheck_Analyzer(shape).IsValid());
            Plate plate;
            plate.id = "synthetic-plate";
            plate.installed = definition;
            plate.manufacturingDimensions = {{"fixture_dimension", 123}};
            OcctView view;
            view.showPlates({plate});
            view.clearPlates();
            QCOMPARE(plate.manufacturingDimensions.at("fixture_dimension"), 123.0);
        }
        InstalledSurface invalid;
        QVERIFY_THROWS_EXCEPTION(std::invalid_argument, makeInstalledShape(invalid));
    }
    void nativeViewerRendersAndNavigates() {
        if (QGuiApplication::platformName() != "windows") QSKIP("Native Windows graphics check runs separately");
        OcctView view;
        view.resize(800, 600);
        view.show();
        QTest::qWait(100);
        QVERIFY2(view.initialized(), qPrintable(view.failure()));
        std::vector<Plate> plates;
        for (auto part : {Part::Bottom, Part::Shell, Part::Roof}) {
            Plate plate;
            plate.id = std::string("fixture-") + std::to_string(static_cast<int>(part));
            plate.part = part;
            plate.manufacturingDimensions = {{"synthetic_dimension", 123.0}};
            plate.installed.kind = part == Part::Bottom ? InstalledSurface::Kind::Plane
                : part == Part::Shell ? InstalledSurface::Kind::Cylinder : InstalledSurface::Kind::Cone;
            plate.installed.radius = 1000;
            plate.installed.coneSemiAngle = -0.4;
            plate.installed.origin.z = part == Part::Roof ? 500.0 : 0.0;
            plate.installed.parameterBoundary = part == Part::Bottom
                ? std::vector<Point2>{{0,0},{1000,0},{1000,500},{0,500}}
                : std::vector<Point2>{{0,0},{0.8,0},{0.8,500},{0,500}};
            plates.push_back(plate);
        }
        view.showPlates(plates);
        QCOMPARE(view.displayedPlateCount(), std::size_t(3));
        QTemporaryDir directory;
        const auto beforePath = directory.filePath("before.png");
        const auto afterPath = directory.filePath("after.png");
        QVERIFY(view.capture(beforePath));
        QTest::mousePress(&view, Qt::LeftButton, Qt::NoModifier, QPoint(350, 250));
        QMouseEvent move(QEvent::MouseMove, QPointF(450, 300), QPointF(450, 300),
            Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
        QCoreApplication::sendEvent(&view, &move);
        QTest::mouseRelease(&view, Qt::LeftButton, Qt::NoModifier, QPoint(450, 300));
        QWheelEvent wheel(QPointF(400, 300), QPointF(400, 300), QPoint(), QPoint(0, 120),
            Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
        QCoreApplication::sendEvent(&view, &wheel);
        QVERIFY(view.capture(afterPath));
        QVERIFY(QImage(beforePath) != QImage(afterPath));
        for (const auto& plate : plates)
            QCOMPARE(plate.manufacturingDimensions.at("synthetic_dimension"), 123.0);
        view.clearPlates();
        QCOMPARE(view.displayedPlateCount(), std::size_t(0));
    }
    void envelopeUsesInputsWithoutInventingManufacturingData() {
        Project project;
        QVERIFY(!envelopeDimensions(project));
        project.setParameter(Part::Shell, "diameter_mm", 6000);
        QVERIFY(!envelopeDimensions(project));
        project.setParameter(Part::Shell, "height_mm", 4000);
        auto dimensions = envelopeDimensions(project);
        QVERIFY(dimensions && !dimensions->roofRiseMm);
        QCOMPARE(makeEnvelopeShapes(*dimensions).size(), std::size_t(2));
        project.setParameter(Part::Roof, "rise_mm", 800);
        const auto revision = project.revision();
        dimensions = envelopeDimensions(project);
        const auto shapes = makeEnvelopeShapes(*dimensions);
        QCOMPARE(shapes.size(), std::size_t(3));
        Bnd_Box bounds;
        for (const auto& component : shapes) {
            QVERIFY(BRepCheck_Analyzer(component.shape).IsValid());
            BRepBndLib::Add(component.shape, bounds);
        }
        double x0, y0, z0, x1, y1, z1;
        bounds.Get(x0, y0, z0, x1, y1, z1);
        QVERIFY(std::abs(x0 + 3000) < 0.01 && std::abs(x1 - 3000) < 0.01);
        QVERIFY(std::abs(z0) < 0.01 && std::abs(z1 - 4800) < 0.01);
        QCOMPARE(project.revision(), revision);
        QVERIFY(project.plates().empty());
        QVERIFY(!project.hasCurrentLayout() && !project.hasCurrentChecks());
        project.setParameter(Part::Roof, "rise_mm", std::nullopt);
        QCOMPARE(makeEnvelopeShapes(*envelopeDimensions(project)).size(), std::size_t(2));
        project.setParameter(Part::Shell, "height_mm", -1);
        QVERIFY_THROWS_EXCEPTION(std::invalid_argument, envelopeDimensions(project));
    }
    void nativeWorkspaceEnvelopeUpdatesAndClears() {
        if (QGuiApplication::platformName() != "windows") QSKIP("Native Windows graphics check runs separately");
        MainWindow window;
        window.show();
        QTest::qWait(100);
        QVERIFY2(window.view()->initialized(), qPrintable(window.view()->failure()));
        window.findChild<QAction*>("previewExampleAction")->trigger();
        QCOMPARE(window.view()->displayedComponentCount(), std::size_t(3));
        QCOMPARE(window.view()->displayedPlateCount(), std::size_t(0));
        QVERIFY(window.project().plates().empty() && !window.project().hasCurrentLayout());
        QTemporaryDir directory;
        const auto firstPath = directory.filePath("first.png");
        const auto changedPath = directory.filePath("changed.png");
        QVERIFY(window.view()->capture(firstPath));
        auto* height = window.findChild<QLineEdit*>("height_mm");
        height->selectAll();
        QTest::keyClicks(height, "8000");
        QTest::keyClick(height, Qt::Key_Return);
        QVERIFY(window.view()->capture(changedPath));
        QVERIFY(QImage(firstPath) != QImage(changedPath));
        QCOMPARE(window.project().parameters(Part::Shell).at("height_mm"), 8000.0);
        auto* tree = window.findChild<QTreeWidget*>("componentTree");
        const auto revision = window.project().revision();
        tree->topLevelItem(0)->child(2)->setCheckState(0, Qt::Unchecked);
        QCOMPARE(window.view()->displayedComponentCount(), std::size_t(2));
        tree->topLevelItem(0)->child(2)->setCheckState(0, Qt::Checked);
        window.view()->setProjection(V3d_Zpos);
        QCOMPARE(window.project().revision(), revision);
        QCOMPARE(window.view()->displayedComponentCount(), std::size_t(3));
        // A failed manufacturing request must not turn the envelope into plate data.
        window.findChild<QAction*>("generateLayoutAction")->trigger();
        QVERIFY(window.project().plates().empty() && !window.project().hasCurrentLayout());
        QCOMPARE(window.view()->displayedComponentCount(), std::size_t(3));
        height->clear();
        QTest::keyClick(height, Qt::Key_Return);
        QCOMPARE(window.view()->displayedComponentCount(), std::size_t(0));
        QVERIFY(!window.project().parameters(Part::Shell).count("height_mm"));
    }
};
QTEST_MAIN(WorkspaceTests)
#include "WorkspaceTests.moc"
