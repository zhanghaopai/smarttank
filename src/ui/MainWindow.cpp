#include "ui/MainWindow.h"
#include "view/OcctView.h"
#include "geometry/EnvelopeGeometry.h"
#include <QHBoxLayout>
#include <QTreeWidget>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QLineEdit>
#include <QLabel>
#include <QScrollArea>
#include <QGroupBox>
#include <QFont>
#include <QMenuBar>
#include <QToolBar>
#include <QStatusBar>
#include <QMessageBox>
#include <QAction>
#include <QLocale>
#include <cmath>
#include <vector>
#include <Standard_Failure.hxx>

namespace smarttank {
namespace {
QString partName(Part part) {
    switch (part) {
    case Part::Bottom: return QStringLiteral("罐底");
    case Part::Shell: return QStringLiteral("罐壁");
    case Part::Roof: return QStringLiteral("自支撑锥顶");
    }
    return {};
}
struct Field { const char* key; QString label; };
std::vector<Field> fields(Part part) {
    switch (part) {
    case Part::Bottom: return {{"thickness_mm", QStringLiteral("厚度（mm）")}};
    case Part::Shell: return {{"diameter_mm", QStringLiteral("罐体直径（mm）")},
                             {"height_mm", QStringLiteral("罐壁高度（mm）")},
                             {"thickness_mm", QStringLiteral("厚度（mm）")}};
    case Part::Roof: return {{"rise_mm", QStringLiteral("锥顶升高（mm）")},
                            {"thickness_mm", QStringLiteral("厚度（mm）")}};
    }
    return {};
}
QString stateName(const ResultStamp& stamp) {
    switch (stamp.state) {
    case ResultState::Missing: return QStringLiteral("未生成");
    case ResultState::Current: return QStringLiteral("当前结果");
    case ResultState::Stale: return QStringLiteral("需重新生成");
    case ResultState::Invalid: return QStringLiteral("不可用");
    }
    return {};
}
}

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setObjectName("mainWindow");
    setWindowTitle(QStringLiteral("SmartTank[*] — 自支撑锥顶储罐"));
    auto bodyFont = font();
    if (bodyFont.pointSizeF() < 12.0) bodyFont.setPointSizeF(12.0);
    setFont(bodyFont);
    resize(1280, 820);

    auto* central = new QWidget(this);
    central->setObjectName("previewPanel");
    auto* centerLayout = new QVBoxLayout(central);
    auto* previewTitle = new QLabel(QStringLiteral("储罐外形预览"), central);
    auto headingFont = font();
    headingFont.setPointSizeF(font().pointSizeF() * 1.125);
    headingFont.setWeight(QFont::Medium);
    previewTitle->setFont(headingFont);
    centerLayout->addWidget(previewTitle);
    designStatus_ = new QLabel(central);
    designStatus_->setObjectName("designStatus");
    designStatus_->setWordWrap(true);
    centerLayout->addWidget(designStatus_);
    view_ = new OcctView(central);
    centerLayout->addWidget(view_, 1);
    auto* navigationHelp = new QLabel(QStringLiteral("旋转：左键拖动 / Alt+方向键\n缩放：滚轮 / Ctrl++、Ctrl+- · Ctrl+0 适合窗口"), central);
    navigationHelp->setWordWrap(true);
    centerLayout->addWidget(navigationHelp);
    resultStatus_ = new QLabel(central);
    resultStatus_->setObjectName("resultStatus");
    resultStatus_->setWordWrap(true);
    centerLayout->addWidget(resultStatus_);
    auto* workspace = new QWidget(this);
    workspace->setObjectName("workspace");
    auto* workspaceLayout = new QHBoxLayout(workspace);
    workspaceLayout->setContentsMargins(0, 0, 0, 0);
    parameterPanel_ = new QWidget(workspace);
    parameterPanel_->setObjectName("parametersPanel");
    auto* panelLayout = new QVBoxLayout(parameterPanel_);
    partTitle_ = new QLabel(parameterPanel_);
    partTitle_->setObjectName("partTitle");
    partTitle_->setFont(headingFont);
    panelLayout->addWidget(partTitle_);
    auto* inputHelp = new QLabel(QStringLiteral("单位：mm。输入后按 Enter 或移出输入框应用；清空可移除参数。"), parameterPanel_);
    inputHelp->setWordWrap(true);
    panelLayout->addWidget(inputHelp);
    form_ = new QFormLayout();
    form_->setRowWrapPolicy(QFormLayout::WrapLongRows);
    form_->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    form_->setVerticalSpacing(12);
    panelLayout->addLayout(form_);
    auto* basisGroup = new QGroupBox(QStringLiteral("工程依据"), parameterPanel_);
    auto* basisLayout = new QVBoxLayout(basisGroup);
    auto* basis = new QLabel(QStringLiteral("标准版次：未确定\n材料：未确定\n排板及制造规则：未确定"), parameterPanel_);
    basis->setObjectName("basisStatus");
    basis->setWordWrap(true);
    basisLayout->addWidget(basis);
    panelLayout->addWidget(basisGroup);
    auto* availability = new QLabel(QStringLiteral("当前可编辑参数并预览外形。保存、排板、校核和制造导出暂不可用。"), parameterPanel_);
    availability->setWordWrap(true);
    panelLayout->addWidget(availability);
    panelLayout->addStretch();
    auto* parameterScroll = new QScrollArea(workspace);
    parameterScroll->setWidgetResizable(true);
    parameterScroll->setFrameShape(QFrame::NoFrame);
    parameterScroll->setWidget(parameterPanel_);
    parameterScroll->setMinimumWidth(280);
    auto* componentsPanel = new QWidget(workspace);
    componentsPanel->setObjectName("componentsPanel");
    auto* componentsLayout = new QVBoxLayout(componentsPanel);
    auto* componentsTitle = new QLabel(QStringLiteral("组件树"), componentsPanel);
    componentsTitle->setFont(headingFont);
    componentsLayout->addWidget(componentsTitle);
    tree_ = new QTreeWidget(componentsPanel);
    tree_->setObjectName("componentTree");
    tree_->setHeaderHidden(true);
    tree_->setAccessibleName(QStringLiteral("储罐组件与可见性"));
    tree_->setToolTip(QStringLiteral("选择组件编辑参数；空格键切换所选组件显隐。"));
    tree_->setMinimumWidth(fontMetrics().horizontalAdvance(QStringLiteral("自支撑锥顶储罐")) + 64);
    auto* root = new QTreeWidgetItem(tree_, {QStringLiteral("自支撑锥顶储罐")});
    for (const auto part : {Part::Bottom, Part::Shell, Part::Roof}) {
        auto* child = new QTreeWidgetItem(root, {partName(part)});
        child->setData(0, Qt::UserRole, static_cast<int>(part));
        child->setCheckState(0, Qt::Checked);
    }
    root->setExpanded(true);
    componentsLayout->addWidget(tree_, 1);
    workspaceLayout->addWidget(parameterScroll, 3);
    workspaceLayout->addWidget(central, 6);
    workspaceLayout->addWidget(componentsPanel, 2);
    setCentralWidget(workspace);

    auto* engineeringMenu = menuBar()->addMenu(QStringLiteral("工程"));
    auto* newAction = engineeringMenu->addAction(QStringLiteral("新建工程"));
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

    auto* designMenu = menuBar()->addMenu(QStringLiteral("设计"));
    auto* layoutAction = designMenu->addAction(QStringLiteral("生成排板"));
    layoutAction->setObjectName("generateLayoutAction");
    connect(layoutAction, &QAction::triggered, this, &MainWindow::generateLayout);
    auto* calculateAction = designMenu->addAction(QStringLiteral("计算校核"));
    calculateAction->setObjectName("calculateAction");
    connect(calculateAction, &QAction::triggered, this, &MainWindow::calculate);
    auto* viewMenu = menuBar()->addMenu(QStringLiteral("视图"));
    auto* exampleAction = viewMenu->addAction(QStringLiteral("载入示例尺寸"));
    exampleAction->setObjectName("previewExampleAction");
    connect(exampleAction, &QAction::triggered, this, &MainWindow::loadPreviewExample);
    auto* fitAction = viewMenu->addAction(QStringLiteral("适合窗口"));
    fitAction->setObjectName("fitViewAction");
    fitAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+0")));
    connect(fitAction, &QAction::triggered, view_, &OcctView::fitAll);
    int projectionKey = Qt::Key_1;
    QList<QAction*> projectionActions;
    for (const auto& item : std::vector<std::pair<QString, V3d_TypeOfOrientation>>{
        {QStringLiteral("等轴测"), V3d_XposYnegZpos},
        {QStringLiteral("正视"), V3d_Yneg}, {QStringLiteral("俯视"), V3d_Zpos}}) {
        auto* action = viewMenu->addAction(item.first);
        action->setShortcut(QKeySequence(Qt::CTRL | projectionKey++));
        projectionActions.append(action);
        connect(action, &QAction::triggered, this, [this, orientation = item.second] {
            view_->setProjection(orientation);
        });
    }
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
    auto* exportMenu = menuBar()->addMenu(QStringLiteral("导出"));
    for (const auto& name : {QStringLiteral("DXF 制造图"), QStringLiteral("CSV 板件明细"), QStringLiteral("计算报告")}) {
        auto* action = exportMenu->addAction(name);
        connect(action, &QAction::triggered, this, &MainWindow::exportUnavailable);
    }
    auto* toolbar = addToolBar(QStringLiteral("工程操作"));
    toolbar->setObjectName("mainToolbar");
    toolbar->addAction(newAction);
    toolbar->setMovable(false);
    toolbar->addAction(exampleAction);
    toolbar->addSeparator();
    toolbar->addAction(fitAction);
    for (auto* action : projectionActions) toolbar->addAction(action);
    connect(tree_, &QTreeWidget::currentItemChanged, this, [this](QTreeWidgetItem* item) {
        if (item && item->data(0, Qt::UserRole).isValid())
            selectPart(static_cast<Part>(item->data(0, Qt::UserRole).toInt()));
    });
    connect(tree_, &QTreeWidget::itemChanged, this, [this](QTreeWidgetItem* item, int) {
        if (item && item->data(0, Qt::UserRole).isValid())
            view_->setPartVisible(static_cast<Part>(item->data(0, Qt::UserRole).toInt()),
                                 item->checkState(0) == Qt::Checked);
    });
    connect(view_, &OcctView::renderingFailed, this, [this](const QString& reason) {
        designStatus_->setText(QStringLiteral("三维视图初始化失败：") + reason);
        statusBar()->showMessage(QStringLiteral("三维视图不可用"));
    });
    tree_->setCurrentItem(root->child(1));
    updatePreview();
    refreshStatus();
}
void MainWindow::selectPart(Part part) {
    selected_ = part;
    partTitle_->setText(partName(part));
    while (form_->rowCount() > 0) form_->removeRow(0);
    for (const auto& field : fields(part)) {
        auto* edit = new QLineEdit(parameterPanel_);
        edit->setObjectName(QString::fromLatin1(field.key));
        edit->setPlaceholderText(QStringLiteral("未输入"));
        edit->setAccessibleName(field.label);
        edit->setMinimumHeight(fontMetrics().height() + 12);
        edit->setClearButtonEnabled(true);
        auto* fieldPanel = new QWidget(parameterPanel_);
        auto* fieldLayout = new QVBoxLayout(fieldPanel);
        fieldLayout->setContentsMargins(0, 0, 0, 0);
        fieldLayout->addWidget(edit);
        auto* error = new QLabel(fieldPanel);
        error->setWordWrap(true);
        error->hide();
        fieldLayout->addWidget(error);
        const auto& values = project_.parameters(part);
        const auto value = values.find(field.key);
        if (value != values.end()) edit->setText(locale().toString(value->second, 'g', 15));
        auto* label = new QLabel(field.label, parameterPanel_);
        label->setBuddy(edit);
        form_->addRow(label, fieldPanel);
        connect(edit, &QLineEdit::editingFinished, this, [this, edit, error, part, key = std::string(field.key)] {
            std::optional<double> value;
            if (!edit->text().trimmed().isEmpty()) {
                bool ok = false;
                const auto number = locale().toDouble(edit->text().trimmed(), &ok);
                if (!ok || !std::isfinite(number) || number <= 0) {
                    error->setText(QStringLiteral("输入错误：请输入大于零的有效数值，或清空。本次输入尚未应用。"));
                    error->show();
                    edit->setAccessibleDescription(error->text());
                    return;
                }
                value = number;
            }
            error->hide();
            edit->setAccessibleDescription({});
            if (project_.setParameter(part, key, value)) {
                setWindowModified(true);
                updatePreview();
                refreshStatus();
            }
        });
    }
    view_->highlightPart(part);
    refreshStatus();
}
void MainWindow::refreshStatus() {
    if (view_->failure().isEmpty())
        designStatus_->setText(previewStatus_);
    resultStatus_->setText(QStringLiteral("排板：%1    校核：%2\n基础校核不包含风、地震、锚固等专项。")
        .arg(stateName(project_.layoutStamp()), stateName(project_.calculationStamp())));
    statusBar()->showMessage(QStringLiteral("工程修订 %1 · %2").arg(project_.revision()).arg(partName(selected_)));
}
void MainWindow::updatePreview() {
    try {
        const auto dimensions = envelopeDimensions(project_);
        if (!dimensions) {
            view_->clearPlates();
            previewStatus_ = QStringLiteral("输入罐体直径和罐壁高度后显示外形；输入锥顶升高后显示罐顶。尚未生成排板。 ");
        } else {
            view_->showEnvelope(*dimensions);
            previewStatus_ = dimensions->roofRiseMm
                ? QStringLiteral("外形预览 · 罐底 / 罐壁 / 自支撑锥顶 · 排板未生成，不能用于制造。")
                : QStringLiteral("外形预览 · 罐底 / 罐壁 · 锥顶升高未输入；排板未生成，不能用于制造。 ");
        }
    } catch (const Standard_Failure& error) {
        view_->clearPlates();
        previewStatus_ = QStringLiteral("无法生成外形预览：") + QString::fromUtf8(error.what());
    } catch (const std::exception& error) {
        view_->clearPlates();
        previewStatus_ = QStringLiteral("无法生成外形预览：") + QString::fromUtf8(error.what());
    }
}
void MainWindow::generateLayout() {
    project_.failLayout("Layout and manufacturing rules are not confirmed");
    updatePreview();
    refreshStatus();
    statusBar()->showMessage(QStringLiteral("无法生成排板：排板及制造规则尚未确定。"));
}
void MainWindow::calculate() {
    project_.failChecks("Standard, material and check rules are not confirmed");
    refreshStatus();
    statusBar()->showMessage(QStringLiteral("无法计算：标准版次、材料及校核规则尚未确定。"));
}
void MainWindow::exportUnavailable() {
    statusBar()->showMessage(QStringLiteral("无法导出：尚无有效制造几何或计算结果。"));
}
void MainWindow::newProject() {
    if (isWindowModified() && QMessageBox::question(this, QStringLiteral("新建工程"),
        QStringLiteral("当前参数尚未保存，是否清空并新建工程？"), QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No) != QMessageBox::Yes) return;
    project_ = Project{};
    updatePreview();
    setWindowModified(false);
    selectPart(selected_);
    refreshStatus();
}
void MainWindow::loadPreviewExample() {
    if (isWindowModified() && QMessageBox::question(this, QStringLiteral("载入示例尺寸"),
        QStringLiteral("是否用示例尺寸替换当前直径、高度和锥顶升高？其余参数保持不变。"),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes) return;
    project_.setParameter(Part::Shell, "diameter_mm", 6000.0);
    project_.setParameter(Part::Shell, "height_mm", 4000.0);
    project_.setParameter(Part::Roof, "rise_mm", 800.0);
    setWindowModified(true);
    selectPart(selected_);
    updatePreview();
    refreshStatus();
    statusBar()->showMessage(QStringLiteral("已载入示例外形尺寸：直径 6000 mm、高度 4000 mm、锥顶升高 800 mm；仅供预览。"));
}
}
