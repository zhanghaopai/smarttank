#pragma once
#include <QWidget>
#include <QPoint>
#include <QString>
#include <QImage>
#include <AIS_InteractiveContext.hxx>
#include <AIS_Shape.hxx>
#include <V3d_View.hxx>
#include <map>
#include "core/Project.h"
#include "view/EnvelopeGeometry.h"

namespace smarttank {
TopoDS_Shape makeInstalledShape(const InstalledSurface& definition);

class OcctView : public QWidget {
    Q_OBJECT
public:
    explicit OcctView(QWidget* parent = nullptr);
    ~OcctView() override;
    bool initialized() const { return !view_.IsNull(); }
    QString failure() const { return failure_; }
    void showPlates(const std::vector<Plate>& plates);
    void showEnvelope(const EnvelopeDimensions& dimensions);
    void clearPlates();
    void fitAll();
    void highlightPart(Part part);
    std::size_t displayedPlateCount() const { return initialized() && !envelope_ ? pending_.size() : 0; }
    std::size_t displayedComponentCount() const;
    void setProjection(V3d_TypeOfOrientation orientation);
    void setPartVisible(Part part, bool visible);
    bool capture(const QString& path);
    QImage snapshot();

signals:
    void renderingFailed(const QString& reason);

protected:
    QPaintEngine* paintEngine() const override;
    void showEvent(QShowEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

private:
    void initialize();
    void displayPending();
    bool native_;
    QString failure_;
    occ::handle<V3d_View> view_;
    occ::handle<AIS_InteractiveContext> context_;
    std::map<std::string, occ::handle<AIS_Shape>> objects_;
    std::map<std::string, Part> parts_;
    std::vector<Plate> pending_;
    std::optional<EnvelopeDimensions> envelope_;
    std::optional<Part> selectedPart_;
    std::array<bool, 3> partVisible_{{true, true, true}};
};
}
