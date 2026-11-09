#include "ui/MainWindow.h"
#include "view/OcctView.h"

#include <QAction>
#include <QMenu>
#include <QMenuBar>
#include <QToolBar>
#include <vector>

namespace smarttank {

void MainWindow::createActions() {
    WorkspaceActions actions;
    createEngineeringMenu(actions);
    createDesignMenu();
    createViewMenu(actions);
    createExportMenu();
    createToolbar(actions);
}

void MainWindow::createEngineeringMenu(WorkspaceActions& actions) {
    auto* engineeringMenu = menuBar()->addMenu(QStringLiteral("工程"));
    auto* newAction = engineeringMenu->addAction(QStringLiteral("新建工程"));
    actions.newProject = newAction;
    newAction->setObjectName("newProjectAction");
    newAction->setShortcut(QKeySequence::New);
    connect(newAction, &QAction::triggered, this, &MainWindow::newProject);
    for (const auto& name : {QStringLiteral("打开工程"), QStringLiteral("保存工程")}) {
        auto* action = engineeringMenu->addAction(name);
        action->setEnabled(false);
        action->setToolTip(QStringLiteral("工程保存与打开暂不可用"));
    }
    auto* exitAction = engineeringMenu->addAction(QStringLiteral("退出"));
    connect(exitAction, &QAction::triggered, this, &QWidget::close);
}

void MainWindow::createDesignMenu() {
    auto* designMenu = menuBar()->addMenu(QStringLiteral("设计"));
    auto* layoutAction = designMenu->addAction(QStringLiteral("生成排板"));
    layoutAction->setObjectName("generateLayoutAction");
    connect(layoutAction, &QAction::triggered, this, &MainWindow::generateLayout);
    auto* calculateAction = designMenu->addAction(QStringLiteral("计算校核"));
    calculateAction->setObjectName("calculateAction");
    connect(calculateAction, &QAction::triggered, this, &MainWindow::calculate);
}

void MainWindow::createViewMenu(WorkspaceActions& actions) {
    auto* viewMenu = menuBar()->addMenu(QStringLiteral("视图"));
    auto* exampleAction = viewMenu->addAction(QStringLiteral("载入示例尺寸"));
    actions.previewExample = exampleAction;
    exampleAction->setObjectName("previewExampleAction");
    connect(exampleAction, &QAction::triggered, this, &MainWindow::loadPreviewExample);
    auto* fitAction = viewMenu->addAction(QStringLiteral("适合窗口"));
    actions.fitView = fitAction;
    fitAction->setObjectName("fitViewAction");
    fitAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+0")));
    connect(fitAction, &QAction::triggered, view_, &OcctView::fitAll);
    int projectionKey = Qt::Key_1;
    for (const auto& item : std::vector<std::pair<QString, V3d_TypeOfOrientation>>{
        {QStringLiteral("等轴测"), V3d_XposYnegZpos},
        {QStringLiteral("正视"), V3d_Yneg}, {QStringLiteral("俯视"), V3d_Zpos}}) {
        auto* action = viewMenu->addAction(item.first);
        action->setShortcut(QKeySequence(Qt::CTRL | projectionKey++));
        actions.projections.append(action);
        connect(action, &QAction::triggered, this, [this, orientation = item.second] {
            view_->setProjection(orientation);
        });
    }
    createNavigationMenu(viewMenu);
}

void MainWindow::createNavigationMenu(QMenu* viewMenu) {
    auto* navigationMenu = viewMenu->addMenu(QStringLiteral("旋转与缩放"));
    const auto addNavigation = [this, navigationMenu](const QString& name, const QString& shortcut, auto operation) {
        auto* action = navigationMenu->addAction(name);
        action->setShortcut(QKeySequence(shortcut));
        connect(action, &QAction::triggered, this, operation);
    };
    addNavigation(QStringLiteral("向左旋转"), QStringLiteral("Alt+Left"), [this] { view_->rotate(-0.1, 0); });
    addNavigation(QStringLiteral("向右旋转"), QStringLiteral("Alt+Right"), [this] { view_->rotate(0.1, 0); });
    addNavigation(QStringLiteral("向上旋转"), QStringLiteral("Alt+Up"), [this] { view_->rotate(0, -0.1); });
    addNavigation(QStringLiteral("向下旋转"), QStringLiteral("Alt+Down"), [this] { view_->rotate(0, 0.1); });
    addNavigation(QStringLiteral("放大"), QStringLiteral("Ctrl++"), [this] { view_->zoom(1.15); });
    addNavigation(QStringLiteral("缩小"), QStringLiteral("Ctrl+-"), [this] { view_->zoom(1.0 / 1.15); });
}

void MainWindow::createExportMenu() {
    auto* exportMenu = menuBar()->addMenu(QStringLiteral("导出"));
    for (const auto& name : {QStringLiteral("DXF 制造图"), QStringLiteral("CSV 板件明细"), QStringLiteral("计算报告")}) {
        auto* action = exportMenu->addAction(name);
        connect(action, &QAction::triggered, this, &MainWindow::exportUnavailable);
    }
}

void MainWindow::createToolbar(const WorkspaceActions& actions) {
    auto* toolbar = addToolBar(QStringLiteral("工程操作"));
    toolbar->setObjectName("mainToolbar");
    toolbar->addAction(actions.newProject);
    toolbar->setMovable(false);
    toolbar->addAction(actions.previewExample);
    toolbar->addSeparator();
    toolbar->addAction(actions.fitView);
    toolbar->addActions(actions.projections);
}

} // namespace smarttank
