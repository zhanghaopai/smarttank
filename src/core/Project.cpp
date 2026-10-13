#include "core/Project.h"
#include <cmath>
#include <stdexcept>
#include <set>
#include <tuple>
#include <utility>

namespace smarttank {
namespace {
std::size_t index(Part part) {
    const auto i = static_cast<std::size_t>(part);
    if (i >= 3) throw std::invalid_argument("Unsupported tank part");
    return i;
}
bool same(const CalculationBasis& a, const CalculationBasis& b) {
    return std::tie(a.standardEdition, a.addenda, a.material, a.materialSource, a.confirmedChecks)
        == std::tie(b.standardEdition, b.addenda, b.material, b.materialSource, b.confirmedChecks);
}
}
const Parameters& Project::parameters(Part part) const { return parameters_.at(index(part)); }
const LayoutDefinition& Project::layout(Part part) const { return layouts_.at(index(part)); }

bool Project::setParameter(Part part, const std::string& key, std::optional<double> value) {
    if (key.empty() || (value && !std::isfinite(*value)))
        throw std::invalid_argument("Parameter must have a name and finite value");
    auto& values = parameters_.at(index(part));
    const auto found = values.find(key);
    if (!value) {
        if (found == values.end()) return false;
        values.erase(found);
    } else {
        if (found != values.end() && found->second == *value) return false;
        values[key] = *value;
    }
    changed();
    return true;
}
void Project::setLayout(Part part, LayoutDefinition definition) {
    for (const auto& entry : definition.inputs)
        if (entry.first.empty() || !std::isfinite(entry.second))
            throw std::invalid_argument("Invalid layout input");
    auto& previous = layouts_.at(index(part));
    if (previous.confirmedRule == definition.confirmedRule && previous.inputs == definition.inputs) return;
    previous = std::move(definition);
    changed();
}
void Project::setBasis(CalculationBasis basis) {
    if (same(basis_, basis)) return;
    basis_ = std::move(basis);
    changed();
}
void Project::changed() {
    ++revision_;
    if (layoutStamp_.state == ResultState::Current) {
        layoutStamp_.state = ResultState::Stale;
        layoutStamp_.reason = "Design inputs changed";
    }
    if (calculationStamp_.state == ResultState::Current) {
        calculationStamp_.state = ResultState::Stale;
        calculationStamp_.reason = "Design inputs changed";
    }
}
bool Project::acceptLayout(std::uint64_t sourceRevision, std::vector<Plate> plates) {
    if (sourceRevision != revision_) return false;
    for (const auto& layout : layouts_) {
        if (!layout.confirmedRule || layout.confirmedRule->empty()) {
            failLayout("Layout rules are incomplete"); return false;
        }
    }
    std::set<std::string> ids, numbers;
    if (plates.empty()) { failLayout("No plates generated"); return false; }
    for (const auto& plate : plates) {
        index(plate.part);
        if (plate.id.empty() || plate.manufacturingNumber.empty()
            || !ids.insert(plate.id).second || !numbers.insert(plate.manufacturingNumber).second) {
            failLayout("Plate IDs and manufacturing numbers must be unique and nonempty");
            return false;
        }
    }
    plates_ = std::move(plates);
    layoutStamp_ = {ResultState::Current, revision_, {}};
    return true;
}
bool Project::acceptChecks(std::uint64_t sourceRevision, std::vector<CheckResult> checks) {
    if (sourceRevision != revision_) return false;
    if (!basis_.standardEdition || basis_.standardEdition->empty()
        || !basis_.addenda || !basis_.material || basis_.material->empty()
        || !basis_.materialSource || basis_.materialSource->empty()
        || !basis_.confirmedChecks || basis_.confirmedChecks->empty()) {
        failChecks("Calculation basis is incomplete");
        return false;
    }
    std::array<bool, 3> covered{};
    if (checks.empty()) { failChecks("No checks performed"); return false; }
    for (const auto& check : checks) {
        if (check.check.empty() || check.outcome == CheckResult::Outcome::MissingBasis) {
            failChecks("Calculation checks are incomplete"); return false;
        }
        covered[index(check.part)] = true;
    }
    if (!covered[0] || !covered[1] || !covered[2]) {
        failChecks("All three tank parts require check results"); return false;
    }
    checks_ = std::move(checks);
    calculationStamp_ = {ResultState::Current, revision_, {}};
    return true;
}
void Project::failLayout(std::string reason) { layoutStamp_ = {ResultState::Invalid, revision_, std::move(reason)}; }
void Project::failChecks(std::string reason) { calculationStamp_ = {ResultState::Invalid, revision_, std::move(reason)}; }
bool Project::hasCurrentLayout() const { return layoutStamp_.state == ResultState::Current && layoutStamp_.revision == revision_; }
bool Project::hasCurrentChecks() const { return calculationStamp_.state == ResultState::Current && calculationStamp_.revision == revision_; }
}
