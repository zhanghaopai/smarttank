#include "view/OcctView.h"
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

namespace smarttank {
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
    displayCachedObjects();
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

void OcctView::fitAll() {
    if (!initialized()) return;
    if (!objects_.empty()) {
        view_->FitAll(0.05, false);
        view_->ZFitAll();
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

void OcctView::setProjection(V3d_TypeOfOrientation orientation) {
    if (!initialized()) return;
    view_->SetProj(orientation);
    fitAll();
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
