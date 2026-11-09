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
#include <QScreen>
#include <QMessageBox>
#include <QAbstractButton>
#include <QTimer>
#include <limits>
#include <BRepBndLib.hxx>
#include <Bnd_Box.hxx>
#include <BRepCheck_Analyzer.hxx>
#include "ui/MainWindow.h"
#include "view/OcctView.h"
#include "geometry/InstalledGeometry.h"
#include "geometry/EnvelopeGeometry.h"
using namespace smarttank;

class WorkspaceTests : public QObject {
    Q_OBJECT
private slots:
    void closingProtectsModifiedAndUnappliedInput() {
        MainWindow window;
        window.show();
        auto* edit = window.findChild<QLineEdit*>("diameter_mm");
        edit->setFocus();
        QTest::keyClicks(edit, "2000");
        const auto rejectClose = [&window] {
            auto* message = window.findChild<QMessageBox*>();
            QVERIFY(message);
            message->button(QMessageBox::No)->click();
        };
        QTimer::singleShot(0, &window, rejectClose);
        QVERIFY(!window.close());
        QVERIFY(window.isVisible());
        QTest::keyClick(edit, Qt::Key_Return);
        QVERIFY(window.isWindowModified());
        QTimer::singleShot(0, &window, rejectClose);
        QVERIFY(!window.close());
        QTimer::singleShot(0, &window, [&window] {
            auto* message = window.findChild<QMessageBox*>();
            QVERIFY(message);
            message->button(QMessageBox::Yes)->click();
        });
        QVERIFY(window.close());
        QVERIFY(!window.isVisible());
        MainWindow cleanWindow;
        cleanWindow.show();
        QVERIFY(cleanWindow.close());
    }
    void parameterRoundTripPreservesPrecisionAndRevision() {
        MainWindow window;
        window.show();
        auto* edit = window.findChild<QLineEdit*>("diameter_mm");
        const double input = 6000.123456789012;
        edit->setText(window.locale().toString(input, 'g', std::numeric_limits<double>::max_digits10));
        QTest::keyClick(edit, Qt::Key_Return);
        const auto revision = window.project().revision();
        auto* tree = window.findChild<QTreeWidget*>("componentTree");
        tree->setCurrentItem(tree->topLevelItem(0)->child(2));
        tree->setCurrentItem(tree->topLevelItem(0)->child(1));
        edit = window.findChild<QLineEdit*>("diameter_mm");
        QCOMPARE(window.locale().toDouble(edit->text()), input);
        QTest::keyClick(edit, Qt::Key_Return);
        QCOMPARE(window.project().parameters(Part::Shell).at("diameter_mm"), input);
        QCOMPARE(window.project().revision(), revision);
    }
    void nativeCachedGeometrySurvivesInitializationAndInvalidReplacement() {
        if (QGuiApplication::platformName() != "windows") QSKIP("Requires native Windows graphics");
        OcctView view;
        view.showEnvelope({3000, 4000, 800});
        view.show();
        QVERIFY(QTest::qWaitForWindowExposed(&view));
        QVERIFY2(view.initialized(), qPrintable(view.failure()));
        QCOMPARE(view.displayedComponentCount(), std::size_t(3));
        QVERIFY_THROWS_EXCEPTION(std::invalid_argument, view.showEnvelope({-1, 4000, 800}));
        QCOMPARE(view.displayedComponentCount(), std::size_t(3));
        Plate valid;
        valid.id = "cached-plate";
        valid.installed.parameterBoundary = {{0, 0}, {100, 0}, {100, 100}, {0, 100}};
        view.showPlates({valid});
        QCOMPARE(view.displayedPlateCount(), std::size_t(1));
        QVERIFY_THROWS_EXCEPTION(std::invalid_argument, view.showPlates({valid, valid}));
        QCOMPARE(view.displayedPlateCount(), std::size_t(1));
        QVERIFY(view.close());
        QVERIFY(!view.initialized());
        view.show();
        QVERIFY(QTest::qWaitForWindowExposed(&view));
        QVERIFY2(view.initialized(), qPrintable(view.failure()));
        QCOMPARE(view.displayedPlateCount(), std::size_t(1));
        QVERIFY(!view.snapshot().isNull());
        view.clearPlates();
        QCOMPARE(view.displayedComponentCount(), std::size_t(0));
    }
    void nativeViewportFillsWidgetAfterResizeAndCapture() {
        if (QGuiApplication::platformName() != "windows") QSKIP("Requires native Windows graphics");
        MainWindow window;
        auto& view = *window.view();
        window.resize(1000, 700);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        QVERIFY2(view.initialized(), qPrintable(view.failure()));
        for (const auto size : {QSize(1280, 820), QSize(1100, 760), QSize(1200, 800)}) {
            window.resize(size);
            QTest::qWait(100);
            QVERIFY(!view.snapshot().isNull());
            QTest::qWait(100);
            // Read the actual native window, not OCCT's offscreen ToPixMap.
            const auto image = view.screen()->grabWindow(view.winId()).toImage();
            QVERIFY(!image.isNull());
            for (const auto point : {QPoint(8, 8), QPoint(image.width() - 9, 8),
                                     QPoint(8, image.height() - 9),
                                     QPoint(image.width() - 9, image.height() - 9)}) {
                const auto color = image.pixelColor(point);
                QVERIFY2(color.blue() > color.red() && color.blue() > color.green(),
                         "Native viewport does not cover all widget corners");
            }
        }
    }
    void invalidInputReportsErrorAndPreservesAppliedValue() {
        MainWindow window;
        window.show();
        auto* diameter = window.findChild<QLineEdit*>("diameter_mm");
        diameter->setText(QStringLiteral("2000"));
        QTest::keyClick(diameter, Qt::Key_Return);
        const auto revision = window.project().revision();
        for (const auto& input : {QStringLiteral("-1"), QStringLiteral("abc"), QStringLiteral("0")}) {
            diameter->setText(input);
            QTest::keyClick(diameter, Qt::Key_Return);
            QCOMPARE(window.project().revision(), revision);
            QCOMPARE(window.project().parameters(Part::Shell).at("diameter_mm"), 2000.0);
            QVERIFY(!diameter->accessibleDescription().isEmpty());
        }
        diameter->setText(window.locale().toString(2500.5, 'f', 1));
        QTest::keyClick(diameter, Qt::Key_Return);
        QCOMPARE(window.project().parameters(Part::Shell).at("diameter_mm"), 2500.5);
        QVERIFY(diameter->accessibleDescription().isEmpty());
    }
    void componentParametersAndViewDoNotChangeInputs() {
        MainWindow window;
        window.show();
        auto* tree = window.findChild<QTreeWidget*>("componentTree");
        QVERIFY(tree);
        QCOMPARE(tree->topLevelItemCount(), 1);
        QCOMPARE(tree->topLevelItem(0)->childCount(), 3);
        QVERIFY(window.findChildren<QDockWidget*>().isEmpty());
        QVERIFY(window.centralWidget()->isAncestorOf(tree));
        QVERIFY(window.centralWidget()->isAncestorOf(window.view()));
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
        QCOMPARE(window.findChild<QLineEdit*>("diameter_mm")->text(), window.locale().toString(2000.0, 'g', 15));
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
        const auto beforeKeyboardNavigation = view.snapshot();
        view.rotate(0.1, -0.1);
        view.zoom(1.15);
        QVERIFY(view.snapshot() != beforeKeyboardNavigation);
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
