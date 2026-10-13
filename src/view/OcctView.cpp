#include "view/OcctView.h"
#include <QGuiApplication>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QPainter>
#include <QShowEvent>
#include <QResizeEvent>
#include <Aspect_DisplayConnection.hxx>
#include <OpenGl_GraphicDriver.hxx>
#include <V3d_Viewer.hxx>
#include <WNT_Window.hxx>
#include <Geom_Plane.hxx>
#include <Geom_CylindricalSurface.hxx>
#include <Geom_ConicalSurface.hxx>
#include <GCE2d_MakeSegment.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepLib.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <Prs3d_Drawer.hxx>
#include <Standard_Failure.hxx>
#include <Image_AlienPixMap.hxx>
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace smarttank {
TopoDS_Shape makeInstalledShape(const InstalledSurface& definition) {
    if (definition.parameterBoundary.size() < 3)
        throw std::invalid_argument("Plate boundary needs at least three vertices");
    const auto& origin = definition.origin;
    if (!std::isfinite(origin.x) || !std::isfinite(origin.y) || !std::isfinite(origin.z))
        throw std::invalid_argument("Invalid installation origin");
    gp_Ax3 axes(gp_Pnt(origin.x, origin.y, origin.z), gp_Dir(0, 0, 1));
    occ::handle<Geom_Surface> surface;
    switch (definition.kind) {
    case InstalledSurface::Kind::Plane:
        surface = new Geom_Plane(axes); break;
    case InstalledSurface::Kind::Cylinder:
        if (!std::isfinite(definition.radius) || definition.radius <= 0)
            throw std::invalid_argument("Cylinder radius must be positive");
        surface = new Geom_CylindricalSurface(axes, definition.radius); break;
    case InstalledSurface::Kind::Cone:
        if (!std::isfinite(definition.radius) || definition.radius <= 0
            || !std::isfinite(definition.coneSemiAngle)
            || std::abs(definition.coneSemiAngle) < 1e-9
            || std::abs(definition.coneSemiAngle) >= 1.5707963267948966)
            throw std::invalid_argument("Invalid conical surface");
        surface = new Geom_ConicalSurface(axes, definition.coneSemiAngle, definition.radius); break;
    default: throw std::invalid_argument("Unsupported installed surface");
    }
    BRepBuilderAPI_MakeWire wire;
    const auto& points = definition.parameterBoundary;
    for (std::size_t i = 0; i < points.size(); ++i) {
        const auto& a = points[i];
        const auto& b = points[(i + 1) % points.size()];
        if (!std::isfinite(a.x) || !std::isfinite(a.y)
            || !std::isfinite(b.x) || !std::isfinite(b.y)
            || std::hypot(a.x - b.x, a.y - b.y) < 1e-9)
            throw std::invalid_argument("Invalid or repeated boundary vertex");
        auto curve = GCE2d_MakeSegment(gp_Pnt2d(a.x, a.y), gp_Pnt2d(b.x, b.y)).Value();
        wire.Add(BRepBuilderAPI_MakeEdge(curve, surface));
    }
    if (!wire.IsDone()) throw std::runtime_error("Unable to construct plate boundary");
    BRepBuilderAPI_MakeFace face(surface, wire.Wire(), true);
    if (!face.IsDone()) throw std::runtime_error("Unable to construct plate surface");
    auto shape = face.Shape();
    if (!BRepLib::BuildCurves3d(shape)) throw std::runtime_error("Unable to construct boundary curves");
    if (!BRepCheck_Analyzer(shape).IsValid()) throw std::invalid_argument("Invalid plate surface or boundary");
    return shape;
}

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
    if (!context_.IsNull()) context_->RemoveAll(false);
    objects_.clear();
    context_.Nullify();
    if (!view_.IsNull()) view_->Remove();
    view_.Nullify();
}
QPaintEngine* OcctView::paintEngine() const {
    return native_ ? nullptr : QWidget::paintEngine();
}
void OcctView::initialize() {
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
    occ::handle<WNT_Window> window = new WNT_Window(reinterpret_cast<Aspect_Handle>(winId()));
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
        failure_ = QString::fromUtf8(error.what());
        if (!view_.IsNull()) view_->Remove();
        view_.Nullify(); context_.Nullify();
        emit renderingFailed(failure_);
    }
    catch (const std::exception& error) {
        failure_ = QString::fromUtf8(error.what());
        if (!view_.IsNull()) view_->Remove();
        view_.Nullify(); context_.Nullify();
        emit renderingFailed(failure_);
    }
}
void OcctView::paintEvent(QPaintEvent*) {
    if (initialized()) { view_->Redraw(); return; }
    if (!native_) {
        QPainter painter(this);
        painter.fillRect(rect(), QColor(31, 38, 48));
        painter.setPen(Qt::white);
        painter.drawText(rect(), Qt::AlignCenter, tr("三维查看器需要 Windows 图形环境"));
    }
}
void OcctView::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    if (initialized()) view_->MustBeResized();
}
void OcctView::showPlates(const std::vector<Plate>& plates) {
    // Validate the complete replacement before changing the current presentation.
    std::map<std::string, occ::handle<AIS_Shape>> replacements;
    for (const auto& plate : plates) {
        if (plate.id.empty() || replacements.count(plate.id))
            throw std::invalid_argument("Display plate IDs must be unique and nonempty");
        replacements.emplace(plate.id, new AIS_Shape(makeInstalledShape(plate.installed)));
    }
    pending_ = plates;
    envelope_.reset();
    if (initialized()) displayPending();
}
void OcctView::showEnvelope(const EnvelopeDimensions& dimensions) {
    // Check geometry first; an envelope is never inserted in Project::plates().
    makeEnvelopeShapes(dimensions);
    pending_.clear();
    envelope_ = dimensions;
    if (initialized()) displayPending();
}
void OcctView::displayPending() {
    std::map<std::string, occ::handle<AIS_Shape>> replacements;
    std::map<std::string, Part> parts;
    if (envelope_) {
        for (const auto& component : makeEnvelopeShapes(*envelope_)) {
            const auto key = "component-preview/" + std::to_string(static_cast<int>(component.part));
            auto object = occ::handle<AIS_Shape>(new AIS_Shape(component.shape));
            // A periodic surface seam is not a manufacturing plate joint.
            object->Attributes()->SetFaceBoundaryDraw(false);
            replacements.emplace(key, object);
            parts.emplace(key, component.part);
        }
    }
    for (const auto& plate : pending_) {
        auto object = occ::handle<AIS_Shape>(new AIS_Shape(makeInstalledShape(plate.installed)));
        object->Attributes()->SetFaceBoundaryDraw(true);
        object->SetColor(Quantity_Color(0.65, 0.72, 0.78, Quantity_TOC_RGB));
        replacements.emplace(plate.id, object);
        parts.emplace(plate.id, plate.part);
    }
    context_->RemoveAll(false);
    objects_ = std::move(replacements);
    parts_ = std::move(parts);
    for (const auto& entry : objects_)
        if (partVisible_.at(static_cast<std::size_t>(parts_.at(entry.first))))
            context_->Display(entry.second, false);
    if (selectedPart_) highlightPart(*selectedPart_);
    fitAll();
}
void OcctView::clearPlates() {
    envelope_.reset();
    pending_.clear(); objects_.clear(); parts_.clear();
    if (!context_.IsNull()) { context_->RemoveAll(false); view_->Redraw(); }
    update();
}
void OcctView::fitAll() {
    if (!initialized()) return;
    if (!objects_.empty()) { view_->FitAll(0.05, false); view_->ZFitAll(); }
    view_->Redraw();
}
void OcctView::highlightPart(Part part) {
    selectedPart_ = part;
    if (!initialized()) return;
    for (const auto& entry : objects_) {
        context_->SetColor(entry.second,
            parts_.at(entry.first) == part ? Quantity_Color(0.95, 0.65, 0.2, Quantity_TOC_RGB)
                : parts_.at(entry.first) == Part::Roof ? Quantity_Color(0.48, 0.62, 0.72, Quantity_TOC_RGB)
                : parts_.at(entry.first) == Part::Bottom ? Quantity_Color(0.38, 0.48, 0.56, Quantity_TOC_RGB)
                : Quantity_Color(0.65, 0.72, 0.78, Quantity_TOC_RGB), false);
    }
    view_->Redraw();
}
std::size_t OcctView::displayedComponentCount() const {
    std::array<bool, 3> seen{};
    for (const auto& part : parts_)
        if (partVisible_.at(static_cast<std::size_t>(part.second)))
            seen.at(static_cast<std::size_t>(part.second)) = true;
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
    for (const auto& entry : objects_) {
        if (parts_.at(entry.first) != part) continue;
        if (visible) context_->Display(entry.second, false);
        else context_->Erase(entry.second, false);
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
    const auto scale = std::clamp(view_->Scale() * std::pow(1.15, event->angleDelta().y() / 120.0), 1e-6, 1e6);
    view_->SetScale(scale);
    view_->Redraw();
    event->accept();
}
bool OcctView::capture(const QString& path) {
    return snapshot().save(path);
}
QImage OcctView::snapshot() {
    if (!initialized()) return {};
    Image_AlienPixMap image;
    if (!view_->ToPixMap(image, width(), height())) return {};
    // Avoid dependence on optional OCCT image encoders by using Qt to write PNG.
    const auto buffer = image.Data();
    const auto format = image.Format();
    if (format != Image_Format_RGB && format != Image_Format_BGR
        && format != Image_Format_RGBA && format != Image_Format_BGRA) return {};
    QImage output(width(), height(), QImage::Format_RGB32);
    const auto bytes = (format == Image_Format_RGB || format == Image_Format_BGR) ? 3 : 4;
    for (int y = 0; y < height(); ++y) {
        const auto row = image.IsTopDown() ? y : height() - 1 - y;
        const auto* rowData = buffer + static_cast<std::size_t>(row) * image.SizeRowBytes();
        for (int x = 0; x < width(); ++x) {
            const auto* pixel = rowData + x * bytes;
            output.setPixel(x, y, format == Image_Format_BGR || format == Image_Format_BGRA
                ? qRgb(pixel[2], pixel[1], pixel[0]) : qRgb(pixel[0], pixel[1], pixel[2]));
        }
    }
    return output;
}
}
