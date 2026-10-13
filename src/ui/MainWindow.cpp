#include "ui/MainWindow.h"
#include "view/OcctView.h"
#include <QDockWidget>
#include <QTreeWidget>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QLineEdit>
#include <QLabel>
#include <QDoubleValidator>
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
class OptionalNumberValidator final : public QDoubleValidator {
public:
    using QDoubleValidator::QDoubleValidator;
    State validate(QString& input, int& position) const override {
        if (input.trimmed().isEmpty()) return Acceptable;
        return QDoubleValidator::validate(input, position);
    }
};
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
    resize(1200, 760);

    auto* central = new QWidget(this);
    auto* centerLayout = new QVBoxLayout(central);
    designStatus_ = new QLabel(central);
    designStatus_->setObjectName("designStatus");
    designStatus_->setWordWrap(true);
    centerLayout->addWidget(designStatus_);
    view_ = new OcctView(central);
    centerLayout->addWidget(view_, 1);
    resultStatus_ = new QLabel(central);
    resultStatus_->setObjectName("resultStatus");
    resultStatus_->setWordWrap(true);
    centerLayout->addWidget(resultStatus_);
    setCentralWidget(central);

    auto* parametersDock = new QDockWidget(QStringLiteral("组件参数"), this);
    parametersDock->setObjectName("parametersDock");
    parametersDock->setAllowedAreas(Qt::LeftDockWidgetArea);
    parameterPanel_ = new QWidget(parametersDock);
    auto* panelLayout = new QVBoxLayout(parameterPanel_);
    partTitle_ = new QLabel(parameterPanel_);
    partTitle_->setObjectName("partTitle");
    panelLayout->addWidget(partTitle_);
    form_ = new QFormLayout();
    panelLayout->addLayout(form_);
    inputStatus_ = new QLabel(parameterPanel_);
    inputStatus_->setWordWrap(true);
    panelLayout->addWidget(inputStatus_);
    auto* basis = new QLabel(QStringLiteral("标准版次：未确定\n材料：未确定\n排板及制造规则：未确定"), parameterPanel_);
    basis->setObjectName("basisStatus");
    basis->setWordWrap(true);
    panelLayout->addWidget(basis);
    panelLayout->addStretch();
    parametersDock->setWidget(parameterPanel_);
    addDockWidget(Qt::LeftDockWidgetArea, parametersDock);

    auto* componentsDock = new QDockWidget(QStringLiteral("组件树"), this);
    componentsDock->setObjectName("componentsDock");
    componentsDock->setAllowedAreas(Qt::RightDockWidgetArea);
    tree_ = new QTreeWidget(componentsDock);
    tree_->setObjectName("componentTree");
    tree_->setHeaderHidden(true);
    auto* root = new QTreeWidgetItem(tree_, {QStringLiteral("自支撑锥顶储罐")});
    for (const auto part : {Part::Bottom, Part::Shell, Part::Roof}) {
        auto* child = new QTreeWidgetItem(root, {partName(part)});
        child->setData(0, Qt::UserRole, static_cast<int>(part));
        child->setCheckState(0, Qt::Checked);
    }
    root->setExpanded(true);
    componentsDock->setWidget(tree_);
    addDockWidget(Qt::RightDockWidgetArea, componentsDock);

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
    connect(fitAction, &QAction::triggered, view_, &OcctView::fitAll);
    for (const auto& item : std::vector<std::pair<QString, V3d_TypeOfOrientation>>{
        {QStringLiteral("等轴测"), V3d_XposYnegZpos},
        {QStringLiteral("正视"), V3d_Yneg}, {QStringLiteral("俯视"), V3d_Zpos}}) {
        auto* action = viewMenu->addAction(item.first);
        connect(action, &QAction::triggered, this, [this, orientation = item.second] {
            view_->setProjection(orientation);
        });
    }
    auto* exportMenu = menuBar()->addMenu(QStringLiteral("导出"));
    for (const auto& name : {QStringLiteral("DXF 制造图"), QStringLiteral("CSV 板件明细"), QStringLiteral("计算报告")}) {
        auto* action = exportMenu->addAction(name);
        connect(action, &QAction::triggered, this, &MainWindow::exportUnavailable);
    }
    auto* toolbar = addToolBar(QStringLiteral("工程操作"));
    toolbar->setObjectName("mainToolbar");
    toolbar->addAction(newAction);
    toolbar->addAction(layoutAction);
    toolbar->addAction(calculateAction);
    toolbar->addAction(fitAction);
    toolbar->addAction(exampleAction);
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
    inputStatus_->clear();
    while (form_->rowCount() > 0) form_->removeRow(0);
    for (const auto& field : fields(part)) {
        auto* edit = new QLineEdit(parameterPanel_);
        edit->setObjectName(QString::fromLatin1(field.key));
        edit->setPlaceholderText(QStringLiteral("未输入"));
        auto* validator = new OptionalNumberValidator(edit);
        validator->setLocale(QLocale::c());
        edit->setValidator(validator);
        const auto& values = project_.parameters(part);
        const auto value = values.find(field.key);
        if (value != values.end()) edit->setText(QString::number(value->second, 'g', 15));
        form_->addRow(field.label, edit);
        connect(edit, &QLineEdit::editingFinished, this, [this, edit, part, key = std::string(field.key)] {
            std::optional<double> value;
            if (!edit->text().trimmed().isEmpty()) {
                bool ok = false;
                const auto number = QLocale::c().toDouble(edit->text().trimmed(), &ok);
                if (!ok || !std::isfinite(number) || number <= 0) {
                    inputStatus_->setText(QStringLiteral("请输入大于零的有效数值；本次输入尚未应用。"));
                    edit->setStyleSheet(QStringLiteral("border: 1px solid #bd3333;"));
                    return;
                }
                value = number;
            }
            edit->setStyleSheet({});
            inputStatus_->clear();
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
