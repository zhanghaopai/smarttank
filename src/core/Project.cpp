#include "core/Project.h"
#include <cmath>
#include <stdexcept>
#include <set>
#include <tuple>
#include <utility>

namespace smarttank {
namespace {
std::size_t partIndex(Part part) {
    const auto index = static_cast<std::size_t>(part);
    if (index >= 3) throw std::invalid_argument("Unsupported tank part");
    return index;
}

bool sameCalculationBasis(const CalculationBasis& a, const CalculationBasis& b) {
    return std::tie(a.standardEdition, a.addenda, a.material, a.materialSource, a.confirmedChecks)
        == std::tie(b.standardEdition, b.addenda, b.material, b.materialSource, b.confirmedChecks);
}

void markStale(ResultStamp& stamp) {
    if (stamp.state != ResultState::Current) return;
    stamp.state = ResultState::Stale;
    stamp.reason = "Design inputs changed";
}

std::optional<std::string> validateLayout(const std::array<LayoutDefinition, 3>& layouts, const std::vector<Plate>& plates) {
    for (const auto& layout : layouts) {
        if (!layout.confirmedRule || layout.confirmedRule->empty()) {
            return std::string("Layout rules are incomplete");
        }
    }
    std::set<std::string> plateIds;
    std::set<std::string> manufacturingNumbers;
    if (plates.empty()) {
        return std::string("No plates generated");
    }
    for (const auto& plate : plates) {
        partIndex(plate.part);
        if (plate.id.empty() || plate.manufacturingNumber.empty()
            || !plateIds.insert(plate.id).second || !manufacturingNumbers.insert(plate.manufacturingNumber).second) {
            return std::string("Plate IDs and manufacturing numbers must be unique and nonempty");
        }
    }
    return std::nullopt;
}

bool hasText(const std::optional<std::string>& value) {
    return value && !value->empty();
}

bool hasCalculationBasis(const CalculationBasis& basis) {
    // An empty addenda value is allowed; missing addenda is not.
    return hasText(basis.standardEdition)
        && basis.addenda.has_value()
        && hasText(basis.material)
        && hasText(basis.materialSource)
        && hasText(basis.confirmedChecks);
}

std::optional<std::string> validateChecks(const CalculationBasis& basis, const std::vector<CheckResult>& checks) {
    if (!hasCalculationBasis(basis)) {
        return std::string("Calculation basis is incomplete");
    }
    std::array<bool, 3> coveredParts{};
    if (checks.empty()) {
        return std::string("No checks performed");
    }
    for (const auto& check : checks) {
        if (check.check.empty() || check.outcome == CheckResult::Outcome::MissingBasis) {
            return std::string("Calculation checks are incomplete");
        }
        coveredParts[partIndex(check.part)] = true;
    }
    if (!coveredParts[0] || !coveredParts[1] || !coveredParts[2]) {
        return std::string("All three tank parts require check results");
    }
    return std::nullopt;
}

}

const Parameters& Project::parameters(Part part) const {
    return parameters_.at(partIndex(part));
}

const LayoutDefinition& Project::layout(Part part) const {
    return layouts_.at(partIndex(part));
}

bool Project::setParameter(Part part, const std::string& key, std::optional<double> value) {
    if (key.empty() || (value && !std::isfinite(*value)))
        throw std::invalid_argument("Parameter must have a name and finite value");
    auto& values = parameters_.at(partIndex(part));
    const auto found = values.find(key);
    if (!value) {
        if (found == values.end()) return false;
        values.erase(found);
    } else {
        if (found != values.end() && found->second == *value) return false;
        values[key] = *value;
    }
    markInputsChanged();
    return true;
}

void Project::setLayout(Part part, LayoutDefinition definition) {
    for (const auto& entry : definition.inputs)
        if (entry.first.empty() || !std::isfinite(entry.second))
            throw std::invalid_argument("Invalid layout input");
    auto& previous = layouts_.at(partIndex(part));
    if (previous.confirmedRule == definition.confirmedRule && previous.inputs == definition.inputs) return;
    previous = std::move(definition);
    markInputsChanged();
}

void Project::setBasis(CalculationBasis basis) {
    if (sameCalculationBasis(basis_, basis)) return;
    basis_ = std::move(basis);
    markInputsChanged();
}

void Project::markInputsChanged() {
    ++revision_;
    markStale(layoutStamp_);
    markStale(calculationStamp_);
}

bool Project::acceptLayout(std::uint64_t sourceRevision, std::vector<Plate> plates) {
    if (sourceRevision != revision_) return false;
    if (const auto error = validateLayout(layouts_, plates)) {
        failLayout(*error);
        return false;
    }
    plates_ = std::move(plates);
    layoutStamp_ = {ResultState::Current, revision_, {}};
    return true;
}

bool Project::acceptChecks(std::uint64_t sourceRevision, std::vector<CheckResult> checks) {
    if (sourceRevision != revision_) return false;
    if (const auto error = validateChecks(basis_, checks)) {
        failChecks(*error);
        return false;
    }
    checks_ = std::move(checks);
    calculationStamp_ = {ResultState::Current, revision_, {}};
    return true;
}

void Project::failLayout(std::string reason) {
    layoutStamp_ = {ResultState::Invalid, revision_, std::move(reason)};
}

void Project::failChecks(std::string reason) {
    calculationStamp_ = {ResultState::Invalid, revision_, std::move(reason)};
}

bool Project::hasCurrentLayout() const {
    return layoutStamp_.state == ResultState::Current && layoutStamp_.revision == revision_;
}

bool Project::hasCurrentChecks() const {
    return calculationStamp_.state == ResultState::Current && calculationStamp_.revision == revision_;
}
}
