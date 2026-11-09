#pragma once
#include <QMainWindow>
#include <QList>
#include "core/Project.h"
class QTreeWidget;
class QLabel;
class QFormLayout;
class QMenu;
class QAction;

namespace smarttank {
class OcctView;
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    const Project& project() const { return project_; }
    OcctView* view() const { return view_; }

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    struct WorkspaceActions {
        QAction* newProject = nullptr;
        QAction* previewExample = nullptr;
        QAction* fitView = nullptr;
        QList<QAction*> projections;
    };

    static QString partName(Part part);
    void createWorkspace();
    QWidget* createPreviewPanel();
    QWidget* createParametersPanel();
    QWidget* createComponentsPanel();
    void createActions();
    void createEngineeringMenu(WorkspaceActions& actions);
    void createDesignMenu();
    void createViewMenu(WorkspaceActions& actions);
    void createNavigationMenu(QMenu* viewMenu);
    void createExportMenu();
    void createToolbar(const WorkspaceActions& actions);
    void connectWorkspaceSignals();
    void addParameterField(Part part, const char* key, const QString& label);
    void selectPart(Part part);
    void refreshStatus();
    void updatePreview();
    void generateLayout();
    void calculate();
    void exportUnavailable();
    void newProject();
    void loadPreviewExample();
    bool hasUnsavedChanges() const;
    bool confirmDiscardChanges(const QString& title, const QString& message);
    void showPreviewFailure(const QString& reason);
    Project project_;
    Part selected_ = Part::Shell;
    QTreeWidget* tree_ = nullptr;
    QWidget* parameterPanel_ = nullptr;
    QFormLayout* form_ = nullptr;
    QLabel* partTitle_ = nullptr;
    QLabel* previewErrorLabel_ = nullptr;
    QLabel* resultStatus_ = nullptr;
    OcctView* view_ = nullptr;
    QString previewError_;
};
}
