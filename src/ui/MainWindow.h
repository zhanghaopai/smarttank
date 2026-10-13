#pragma once
#include <QMainWindow>
#include <QMap>
#include "core/Project.h"
class QTreeWidget;
class QLineEdit;
class QLabel;
class QFormLayout;

namespace smarttank {
class OcctView;
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    const Project& project() const { return project_; }
    OcctView* view() const { return view_; }

private:
    void selectPart(Part part);
    void refreshStatus();
    void updatePreview();
    void generateLayout();
    void calculate();
    void exportUnavailable();
    void newProject();
    void loadPreviewExample();
    Project project_;
    Part selected_ = Part::Shell;
    QTreeWidget* tree_;
    QWidget* parameterPanel_;
    QFormLayout* form_;
    QLabel* partTitle_;
    QLabel* designStatus_;
    QLabel* resultStatus_;
    QLabel* inputStatus_;
    OcctView* view_;
    QString previewStatus_;
};
}
