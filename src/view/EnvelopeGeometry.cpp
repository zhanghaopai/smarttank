#include "view/EnvelopeGeometry.h"
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <BRepPrimAPI_MakeCone.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <gp_Circ.hxx>
#include <gp_Ax2.hxx>
#include <cmath>
#include <stdexcept>

namespace smarttank {
namespace {
std::optional<double> parameter(const Project& project, Part part, const char* key) {
    const auto& parameters = project.parameters(part);
    const auto found = parameters.find(key);
    return found == parameters.end() ? std::nullopt : std::optional<double>(found->second);
}
void requirePositive(double value) {
    if (!std::isfinite(value) || value <= 0)
        throw std::invalid_argument("Preview dimensions must be finite and positive");
}
}
std::optional<EnvelopeDimensions> envelopeDimensions(const Project& project) {
    const auto diameter = parameter(project, Part::Shell, "diameter_mm");
    const auto height = parameter(project, Part::Shell, "height_mm");
    if (!diameter || !height) return std::nullopt;
    requirePositive(*diameter);
    requirePositive(*height);
    const auto rise = parameter(project, Part::Roof, "rise_mm");
    if (rise) requirePositive(*rise);
    return EnvelopeDimensions{*diameter / 2.0, *height, rise};
}
std::vector<ComponentShape> makeEnvelopeShapes(const EnvelopeDimensions& dimensions) {
    requirePositive(dimensions.radiusMm);
    requirePositive(dimensions.wallHeightMm);
    if (dimensions.roofRiseMm) requirePositive(*dimensions.roofRiseMm);
    const gp_Ax2 baseAxes(gp_Pnt(0, 0, 0), gp_Dir(0, 0, 1));
    BRepBuilderAPI_MakeWire bottomWire(BRepBuilderAPI_MakeEdge(gp_Circ(baseAxes, dimensions.radiusMm)));
    std::vector<ComponentShape> shapes = {
        {Part::Bottom, BRepBuilderAPI_MakeFace(bottomWire.Wire(), true).Face()},
        {Part::Shell, BRepPrimAPI_MakeCylinder(baseAxes, dimensions.radiusMm, dimensions.wallHeightMm).Face()}
    };
    if (dimensions.roofRiseMm) {
        const gp_Ax2 roofAxes(gp_Pnt(0, 0, dimensions.wallHeightMm), gp_Dir(0, 0, 1));
        shapes.push_back({Part::Roof,
            BRepPrimAPI_MakeCone(roofAxes, dimensions.radiusMm, 0, *dimensions.roofRiseMm).Face()});
    }
    for (const auto& component : shapes)
        if (component.shape.IsNull() || !BRepCheck_Analyzer(component.shape).IsValid())
            throw std::runtime_error("Unable to build valid preview geometry");
    return shapes;
}
}
