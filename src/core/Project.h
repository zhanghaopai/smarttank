#pragma once

#include <array>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace smarttank {

enum class Part { Bottom, Shell, Roof };
using Parameters = std::map<std::string, double>;

struct Point2 { double x = 0; double y = 0; };
struct Point3 { double x = 0; double y = 0; double z = 0; };

// Analytic installation coordinates, not tessellated rendering data.
struct InstalledSurface {
    enum class Kind { Plane, Cylinder, Cone };
    Kind kind = Kind::Plane;
    Point3 origin;
    double radius = 0;
    double coneSemiAngle = 0;
    std::vector<Point2> parameterBoundary;
};

// Manufacturing contours are generated separately from the installed surface.
struct ContourSegment {
    enum class Kind { Line, CircularArc };
    Kind kind = Kind::Line;
    Point2 start, end, center;
    bool counterClockwise = true;
};

struct Plate {
    std::string id;
    Part part = Part::Bottom;
    std::string manufacturingNumber;
    std::optional<std::string> material;
    std::optional<double> thicknessMm;
    InstalledSurface installed;
    std::vector<ContourSegment> manufacturingContour;
    Parameters manufacturingDimensions;
};

struct LayoutDefinition {
    std::optional<std::string> confirmedRule;
    Parameters inputs;
};

struct CalculationBasis {
    std::optional<std::string> standardEdition;
    std::optional<std::string> addenda;
    std::optional<std::string> material;
    std::optional<std::string> materialSource;
    std::optional<std::string> confirmedChecks;
};

enum class ResultState { Missing, Current, Stale, Invalid };
struct ResultStamp {
    ResultState state = ResultState::Missing;
    std::uint64_t revision = 0;
    std::string reason;
};
struct CheckResult {
    Part part;
    std::string check;
    enum class Outcome { Passed, Failed, NotApplicable, MissingBasis };
    Outcome outcome = Outcome::MissingBasis;
    std::string explanation;
};

class Project {
public:
    const Parameters& parameters(Part part) const;
    bool setParameter(Part part, const std::string& key, std::optional<double> value);
    const LayoutDefinition& layout(Part part) const;
    void setLayout(Part part, LayoutDefinition definition);
    const CalculationBasis& basis() const { return basis_; }
    void setBasis(CalculationBasis basis);
    std::uint64_t revision() const { return revision_; }
    const std::vector<Plate>& plates() const { return plates_; }
    const std::vector<CheckResult>& checks() const { return checks_; }
    const ResultStamp& layoutStamp() const { return layoutStamp_; }
    const ResultStamp& calculationStamp() const { return calculationStamp_; }
    bool acceptLayout(std::uint64_t sourceRevision, std::vector<Plate> plates);
    bool acceptChecks(std::uint64_t sourceRevision, std::vector<CheckResult> checks);
    void failLayout(std::string reason);
    void failChecks(std::string reason);
    bool hasCurrentLayout() const;
    bool hasCurrentChecks() const;

private:
    void changed();
    std::array<Parameters, 3> parameters_;
    std::array<LayoutDefinition, 3> layouts_;
    CalculationBasis basis_;
    std::uint64_t revision_ = 0;
    std::vector<Plate> plates_;
    std::vector<CheckResult> checks_;
    ResultStamp layoutStamp_;
    ResultStamp calculationStamp_;
};
}
