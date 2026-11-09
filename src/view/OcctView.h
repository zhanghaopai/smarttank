#pragma once
#include <QWidget>
#include <QPoint>
#include <QString>
#include <QImage>
#include <QPointer>
#include <AIS_InteractiveContext.hxx>
#include <AIS_Shape.hxx>
#include <V3d_View.hxx>
#include <map>
#include "core/Project.h"
#include "geometry/EnvelopeGeometry.h"
class QWindow;

namespace smarttank {
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
    void rotate(double horizontalRadians, double verticalRadians);
    void zoom(double factor);
    void highlightPart(Part part);
    std::size_t displayedPlateCount() const { return initialized() && !displayingEnvelope_ ? objects_.size() : 0; }
    std::size_t displayedComponentCount() const;
    void setProjection(V3d_TypeOfOrientation orientation);
    void setPartVisible(Part part, bool visible);
    bool capture(const QString& path);
    QImage snapshot();

signals:
    void renderingFailed(const QString& reason);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    QPaintEngine* paintEngine() const override;
    void showEvent(QShowEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

private:
    struct DisplayedObject {
        Part part;
        occ::handle<AIS_Shape> presentation;
    };

    void initialize();
    void releaseViewer();
    void handleInitializationFailure(const QString& reason);
    void synchronizeViewport();
    void displayPending();
    void replaceObjects(std::map<std::string, DisplayedObject> replacements, bool envelope);
    bool native_;
    QString failure_;
    QPointer<QWindow> surfaceWindow_;
    occ::handle<V3d_View> view_;
    occ::handle<AIS_InteractiveContext> context_;
    std::map<std::string, DisplayedObject> objects_;
    bool displayingEnvelope_ = false;
    std::optional<Part> selectedPart_ = std::nullopt;
    std::array<bool, 3> partVisible_{{true, true, true}};
};
}
