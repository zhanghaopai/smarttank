#include "view/OcctView.h"
#include "geometry/InstalledGeometry.h"

#include <Prs3d_Drawer.hxx>
#include <algorithm>
#include <stdexcept>
#include <utility>

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
    replaceObjects(std::move(replacements), PresentationKind::Plates);
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
    replaceObjects(std::move(replacements), PresentationKind::Envelope);
}

void OcctView::replaceObjects(std::map<std::string, DisplayedObject> replacements, PresentationKind kind) {
    if (initialized()) context_->RemoveAll(false);
    objects_ = std::move(replacements);
    presentationKind_ = kind;
    if (initialized()) displayCachedObjects();
}

void OcctView::displayCachedObjects() {
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
    presentationKind_ = PresentationKind::Plates;
    objects_.clear();
    if (!context_.IsNull()) {
        context_->RemoveAll(false);
        view_->Redraw();
    }
    update();
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

std::size_t OcctView::displayedComponentCount() const {
    if (!initialized()) return 0;
    std::array<bool, 3> seen{};
    for (const auto& [id, object] : objects_) {
        const auto partIndex = static_cast<std::size_t>(object.part);
        if (partVisible_.at(partIndex)) seen.at(partIndex) = true;
    }
    return static_cast<std::size_t>(std::count(seen.begin(), seen.end(), true));
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

} // namespace smarttank
