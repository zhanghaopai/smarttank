#include "core/Project.h"
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace smarttank;
namespace {
void require(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
void fixtureRules(Project& project) {
    for (auto part : {Part::Bottom, Part::Shell, Part::Roof})
        project.setLayout(part, {std::string("synthetic-test-rule"), {}});
}
Plate fixturePlate() {
    Plate plate;
    plate.id = "test-plate-1";
    plate.manufacturingNumber = "TEST-1";
    plate.part = Part::Shell;
    plate.material = "synthetic-test-material";
    plate.thicknessMm = 6;
    plate.installed.kind = InstalledSurface::Kind::Cylinder;
    plate.installed.radius = 1000;
    plate.installed.parameterBoundary = {{0, 0}, {0.5, 0}, {0.5, 1000}, {0, 1000}};
    plate.manufacturingDimensions = {{"width_mm", 500}, {"height_mm", 1000}};
    return plate;
}
}
int main() {
    try {
        Project project;
        require(!project.basis().standardEdition && !project.basis().material, "No unconfirmed basis defaults");
        require(project.plates().empty() && !project.hasCurrentLayout(), "New project cannot manufacture");
        require(!project.acceptLayout(project.revision(), {fixturePlate()}), "Missing layout rules must block results");
        fixtureRules(project);
        require(project.setParameter(Part::Shell, "diameter_mm", 2000), "Parameter change recorded");
        const auto inputRevision = project.revision();
        require(!project.setParameter(Part::Shell, "diameter_mm", 2000), "No-op edit must not invalidate");
        require(project.revision() == inputRevision, "No-op keeps revision");
        require(project.acceptLayout(inputRevision, {fixturePlate()}), "Layout accepts current provider result");
        const auto& plate = project.plates().front();
        require(plate.id == "test-plate-1" && plate.manufacturingNumber == "TEST-1"
            && plate.material == "synthetic-test-material" && plate.thicknessMm == 6
            && plate.installed.radius == 1000 && plate.manufacturingDimensions.at("width_mm") == 500,
            "Identity links installation, manufacturing dimensions and material");
        project.setParameter(Part::Shell, "diameter_mm", 2400);
        require(project.layoutStamp().state == ResultState::Stale && !project.hasCurrentLayout(), "Input edit invalidates layout");
        require(!project.acceptLayout(inputRevision, {fixturePlate()}), "Delayed old result must be rejected");
        require(project.plates().front().manufacturingDimensions.at("width_mm") == 500, "Old data retained only as stale history");
        project.failLayout("out of rule range");
        require(!project.hasCurrentLayout() && project.layoutStamp().reason == "out of rule range", "Failure cannot reuse old layout");
        auto duplicate = fixturePlate();
        require(!project.acceptLayout(project.revision(), {duplicate, duplicate}), "Duplicate identities rejected");
        require(!project.acceptChecks(project.revision(), {{Part::Bottom, "test", CheckResult::Outcome::Passed, "synthetic"}}),
            "Missing basis cannot produce passing results");
        project.setBasis({"synthetic-edition", "none", "synthetic-material", "test-fixture", "test-checks"});
        std::vector<CheckResult> checks = {
            {Part::Bottom, "test-bottom", CheckResult::Outcome::Passed, "synthetic"},
            {Part::Shell, "test-shell", CheckResult::Outcome::Failed, "synthetic"},
            {Part::Roof, "test-roof", CheckResult::Outcome::NotApplicable, "synthetic"}};
        require(project.acceptChecks(project.revision(), checks) && project.hasCurrentChecks(), "Checks track current design");
        const auto resultRevision = project.revision();
        project.setParameter(Part::Roof, "thickness_mm", 8);
        require(!project.hasCurrentChecks() && project.calculationStamp().state == ResultState::Stale, "Edit invalidates checks");
        require(!project.acceptChecks(resultRevision, checks), "Delayed calculation rejected");
        require(project.setParameter(Part::Roof, "thickness_mm", std::nullopt), "Clearing a parameter is a design change");
        const auto revisionBeforeBadValue = project.revision();
        bool rejected = false;
        try { project.setParameter(Part::Roof, "thickness_mm", std::numeric_limits<double>::quiet_NaN()); }
        catch (const std::invalid_argument&) { rejected = true; }
        require(rejected && project.revision() == revisionBeforeBadValue, "Invalid input must not corrupt project");
        std::cout << "Project identity, missing basis and result invalidation checks passed\n";
        return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
