#include "view/OcctView.h"
#include "geometry/InstalledGeometry.h"
#include <QGuiApplication>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QPainter>
#include <QShowEvent>
#include <QResizeEvent>
#include <QWindow>
#include <QPlatformSurfaceEvent>
#include <Aspect_DisplayConnection.hxx>
#include <OpenGl_GraphicDriver.hxx>
#include <V3d_Viewer.hxx>
#include <Aspect_NeutralWindow.hxx>
#include <Prs3d_Drawer.hxx>
#include <Standard_Failure.hxx>
#include <Image_AlienPixMap.hxx>
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace smarttank {
namespace {
Quantity_Color componentColor(Part part, bool selected) {
    if (selected) return Quantity_Color(0.95, 0.65, 0.2, Quantity_TOC_RGB);
    switch (part) {
    case Part::Roof: return Quantity_Color(0.48, 0.62, 0.72, Quantity_TOC_RGB);
    case Part::Bottom: return Quantity_Color(0.38, 0.48, 0.56, Quantity_TOC_RGB);
    case Part::Shell: return Quantity_Color(0.65, 0.72, 0.78, Quantity_TOC_RGB);
    }
    return Quantity_Color(0.65, 0.72, 0.78, Quantity_TOC_RGB);
}
} // namespace

OcctView::OcctView(QWidget* parent)
    : QWidget(parent), native_(QGuiApplication::platformName() == "windows") {
    setObjectName("occtView");
    setMinimumSize(400, 300);
    if (native_) {
        setAttribute(Qt::WA_NativeWindow);
        setAttribute(Qt::WA_PaintOnScreen);
        setAttribute(Qt::WA_NoSystemBackground);
    }
}
OcctView::~OcctView() {
    if (surfaceWindow_) surfaceWindow_->removeEventFilter(this);
    releaseViewer();
}

void OcctView::releaseViewer() {
    if (!context_.IsNull()) context_->RemoveAll(false);
    context_.Nullify();
    if (!view_.IsNull()) view_->Remove();
    view_.Nullify();
}

bool OcctView::eventFilter(QObject* watched, QEvent* event) {
    if (watched == surfaceWindow_ && event->type() == QEvent::PlatformSurface) {
        const auto* surfaceEvent = static_cast<QPlatformSurfaceEvent*>(event);
        if (surfaceEvent->surfaceEventType() == QPlatformSurfaceEvent::SurfaceAboutToBeDestroyed) {
            // Release OpenGL resources while the native drawable still exists.
            releaseViewer();
        }
    }
    return QWidget::eventFilter(watched, event);
}

QPaintEngine* OcctView::paintEngine() const {
    return native_ ? nullptr : QWidget::paintEngine();
}

void OcctView::initialize() {
    const auto nativeHandle = winId();
    if (surfaceWindow_) surfaceWindow_->removeEventFilter(this);
    surfaceWindow_ = windowHandle();
    surfaceWindow_->installEventFilter(this);
    occ::handle<Aspect_DisplayConnection> display = new Aspect_DisplayConnection();
    occ::handle<OpenGl_GraphicDriver> driver = new OpenGl_GraphicDriver(display);
    occ::handle<V3d_Viewer> viewer = new V3d_Viewer(driver);
    viewer->SetDefaultLights();
    viewer->SetLightOn();
    context_ = new AIS_InteractiveContext(viewer);
    context_->DefaultDrawer()->SetDeviationCoefficient(0.0001);
    context_->DefaultDrawer()->SetDeviationAngle(0.12);
    context_->SetDisplayMode(AIS_Shaded, false);
    view_ = viewer->CreateView();
    view_->ChangeRenderingParams().NbMsaaSamples = 4;
    // Qt owns the HWND and its geometry; OCCT needs physical pixel dimensions.
    occ::handle<Aspect_NeutralWindow> window = new Aspect_NeutralWindow();
    window->SetNativeHandle(reinterpret_cast<Aspect_Drawable>(nativeHandle));
    window->SetSize(qRound(width() * devicePixelRatioF()), qRound(height() * devicePixelRatioF()));
    view_->SetWindow(window);
    view_->SetBackgroundColor(Quantity_Color(0.12, 0.15, 0.19, Quantity_TOC_RGB));
    view_->SetProj(V3d_XposYnegZpos);
    view_->TriedronDisplay(Aspect_TOTP_LEFT_LOWER, Quantity_NOC_GRAY80, 0.08, V3d_ZBUFFER);
    view_->MustBeResized();
    displayPending();
}

void OcctView::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    if (!native_ || initialized() || !failure_.isEmpty()) return;
    try { initialize(); }
    catch (const Standard_Failure& error) {
        handleInitializationFailure(QString::fromUtf8(error.what()));
    }
    catch (const std::exception& error) {
        handleInitializationFailure(QString::fromUtf8(error.what()));
    }
}

void OcctView::handleInitializationFailure(const QString& reason) {
    failure_ = reason;
    releaseViewer();
    emit renderingFailed(failure_);
}

void OcctView::paintEvent(QPaintEvent*) {
    if (initialized()) {
        synchronizeViewport();
        view_->Redraw();
        return;
    }
    if (!native_) {
        QPainter painter(this);
        painter.fillRect(rect(), QColor(31, 38, 48));
        painter.setPen(Qt::white);
        painter.drawText(rect(), Qt::AlignCenter, tr("三维查看器需要 Windows 图形环境"));
    }
}

void OcctView::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    if (initialized()) {
        synchronizeViewport();
        update();
    }
}

void OcctView::synchronizeViewport() {
    if (!initialized()) return;
    const auto window = occ::handle<Aspect_NeutralWindow>::DownCast(view_->Window());
    // Recheck at paint time: Qt may deliver resize before updating the native child.
    // This also covers moving the window between screens with different DPI.
    if (window->SetSize(qRound(width() * devicePixelRatioF()), qRound(height() * devicePixelRatioF())))
        view_->MustBeResized();
}

void OcctView::showPlates(const std::vector<Plate>& plates) {
    // Build and validate the entire replacement before changing display state.
    std::map<std::string, DisplayedObject> replacements;
    for (const auto& plate : plates) {
        if (plate.id.empty() || replacements.count(plate.id)) {
            throw std::invalid_argument("Display plate IDs must be unique and nonempty");
        }
        if (static_cast<std::size_t>(plate.part) >= partVisible_.size()) {
            throw std::invalid_argument("Unsupported tank part");
        }
        auto presentation = occ::handle<AIS_Shape>(new AIS_Shape(makeInstalledShape(plate.installed)));
        presentation->Attributes()->SetFaceBoundaryDraw(true);
        presentation->SetColor(componentColor(Part::Shell, false));
        replacements.emplace(plate.id, DisplayedObject{plate.part, presentation});
    }
    replaceObjects(std::move(replacements), false);
}

void OcctView::showEnvelope(const EnvelopeDimensions& dimensions) {
    std::map<std::string, DisplayedObject> replacements;
    for (const auto& component : makeEnvelopeShapes(dimensions)) {
        const auto key = "component-preview/" + std::to_string(static_cast<int>(component.part));
        auto presentation = occ::handle<AIS_Shape>(new AIS_Shape(component.shape));
        // A periodic surface seam is not a manufacturing plate joint.
        presentation->Attributes()->SetFaceBoundaryDraw(false);
        replacements.emplace(key, DisplayedObject{component.part, presentation});
    }
    replaceObjects(std::move(replacements), true);
}

void OcctView::replaceObjects(std::map<std::string, DisplayedObject> replacements, bool envelope) {
    if (initialized()) context_->RemoveAll(false);
    objects_ = std::move(replacements);
    displayingEnvelope_ = envelope;
    if (initialized()) displayPending();
}

void OcctView::displayPending() {
    // Validated presentations are cached even before the native view is created.
    for (const auto& [id, object] : objects_) {
        if (partVisible_.at(static_cast<std::size_t>(object.part))) {
            context_->Display(object.presentation, false);
        }
    }
    if (selectedPart_) highlightPart(*selectedPart_);
    fitAll();
}

void OcctView::clearPlates() {
    displayingEnvelope_ = false;
    objects_.clear();
    if (!context_.IsNull()) {
        context_->RemoveAll(false);
        view_->Redraw();
    }
    update();
}

void OcctView::fitAll() {
    if (!initialized()) return;
    if (!objects_.empty()) {
        view_->FitAll(0.05, false);
        view_->ZFitAll();
    }
    view_->Redraw();
}

void OcctView::highlightPart(Part part) {
    selectedPart_ = part;
    if (!initialized()) return;
    for (const auto& [id, object] : objects_) {
        const auto color = componentColor(object.part, object.part == part);
        context_->SetColor(object.presentation, color, false);
    }
    view_->Redraw();
}

void OcctView::rotate(double horizontalRadians, double verticalRadians) {
    if (!initialized()) return;
    view_->Rotate(verticalRadians, horizontalRadians, 0.0);
    view_->Redraw();
}

void OcctView::zoom(double factor) {
    if (!initialized()) return;
    view_->SetScale(std::clamp(view_->Scale() * factor, 1e-6, 1e6));
    view_->Redraw();
}

std::size_t OcctView::displayedComponentCount() const {
    if (!initialized()) return 0;
    std::array<bool, 3> seen{};
    for (const auto& [id, object] : objects_) {
        const auto partIndex = static_cast<std::size_t>(object.part);
        if (partVisible_.at(partIndex)) seen.at(partIndex) = true;
    }
    return static_cast<std::size_t>(std::count(seen.begin(), seen.end(), true));
}

void OcctView::setProjection(V3d_TypeOfOrientation orientation) {
    if (!initialized()) return;
    view_->SetProj(orientation);
    fitAll();
}

void OcctView::setPartVisible(Part part, bool visible) {
    partVisible_.at(static_cast<std::size_t>(part)) = visible;
    if (!initialized()) return;
    for (const auto& [id, object] : objects_) {
        if (object.part != part) continue;
        if (visible) {
            context_->Display(object.presentation, false);
        } else {
            context_->Erase(object.presentation, false);
        }
    }
    view_->Redraw();
}

void OcctView::mousePressEvent(QMouseEvent* event) {
    if (initialized() && event->button() == Qt::LeftButton)
        view_->StartRotation(qRound(event->position().x() * devicePixelRatioF()),
                             qRound(event->position().y() * devicePixelRatioF()));
}

void OcctView::mouseMoveEvent(QMouseEvent* event) {
    if (initialized() && (event->buttons() & Qt::LeftButton))
        view_->Rotation(qRound(event->position().x() * devicePixelRatioF()),
                        qRound(event->position().y() * devicePixelRatioF()));
}

void OcctView::wheelEvent(QWheelEvent* event) {
    if (!initialized()) return;
    zoom(std::pow(1.15, event->angleDelta().y() / 120.0));
    event->accept();
}

bool OcctView::capture(const QString& path) {
    return snapshot().save(path);
}

QImage OcctView::snapshot() {
    if (!initialized()) return {};
    Image_AlienPixMap image;
    if (!view_->ToPixMap(image, width(), height())) return {};
    // Offscreen capture uses logical pixels; restore the on-screen viewport.
    view_->MustBeResized();
    update();
    // Avoid dependence on optional OCCT image encoders by using Qt to write PNG.
    const auto buffer = image.Data();
    const auto format = image.Format();
    if (format != Image_Format_RGB && format != Image_Format_BGR
        && format != Image_Format_RGBA && format != Image_Format_BGRA) return {};
    QImage output(width(), height(), QImage::Format_RGB32);
    const bool hasAlpha = format == Image_Format_RGBA || format == Image_Format_BGRA;
    const bool blueFirst = format == Image_Format_BGR || format == Image_Format_BGRA;
    const int bytesPerPixel = hasAlpha ? 4 : 3;
    for (int y = 0; y < height(); ++y) {
        const auto sourceRow = image.IsTopDown() ? y : height() - 1 - y;
        const auto* rowData = buffer + static_cast<std::size_t>(sourceRow) * image.SizeRowBytes();
        for (int x = 0; x < width(); ++x) {
            const auto* pixel = rowData + x * bytesPerPixel;
            output.setPixel(x, y, blueFirst
                ? qRgb(pixel[2], pixel[1], pixel[0]) : qRgb(pixel[0], pixel[1], pixel[2]));
        }
    }
    return output;
}
}
