#include "geometry/InstalledGeometry.h"
#include <Geom_Plane.hxx>
#include <Geom_CylindricalSurface.hxx>
#include <Geom_ConicalSurface.hxx>
#include <GCE2d_MakeSegment.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepLib.hxx>
#include <BRepCheck_Analyzer.hxx>
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

}
