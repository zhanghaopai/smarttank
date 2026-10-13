#pragma once
#include "core/Project.h"
#include <TopoDS_Shape.hxx>
#include <optional>
#include <vector>

namespace smarttank {
// Display envelope only: no thickness, plate IDs or manufacturing dimensions.
struct EnvelopeDimensions {
    double radiusMm;
    double wallHeightMm;
    std::optional<double> roofRiseMm;
};
struct ComponentShape {
    Part part;
    TopoDS_Shape shape;
};
std::optional<EnvelopeDimensions> envelopeDimensions(const Project& project);
std::vector<ComponentShape> makeEnvelopeShapes(const EnvelopeDimensions& dimensions);
}
