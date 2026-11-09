#include "ui/MainWindow.h"
#include "view/OcctView.h"

#include <QFont>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QScrollArea>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace smarttank {
namespace {
QFont sectionHeadingFont(const QWidget* widget) {
    auto font = widget->font();
    font.setPointSizeF(font.pointSizeF() * 1.125);
    font.setWeight(QFont::Medium);
    return font;
}
} // namespace

void MainWindow::createWorkspace() {
    auto* workspace = new QWidget(this);
    workspace->setObjectName("workspace");
    auto* layout = new QHBoxLayout(workspace);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(createParametersPanel(), 3);
    layout->addWidget(createPreviewPanel(), 6);
    layout->addWidget(createComponentsPanel(), 2);
    setCentralWidget(workspace);
}

QWidget* MainWindow::createPreviewPanel() {
    auto* previewPanel = new QWidget(this);
    previewPanel->setObjectName("previewPanel");
    auto* previewLayout = new QVBoxLayout(previewPanel);
    auto* previewTitle = new QLabel(QStringLiteral("储罐外形预览"), previewPanel);
    previewTitle->setFont(sectionHeadingFont(this));
    previewLayout->addWidget(previewTitle);
    designStatus_ = new QLabel(previewPanel);
    designStatus_->setObjectName("designStatus");
    designStatus_->setWordWrap(true);
    designStatus_->hide();
    previewLayout->addWidget(designStatus_);
    view_ = new OcctView(previewPanel);
    previewLayout->addWidget(view_, 1);
    auto* navigationHelp = new QLabel(QStringLiteral("旋转：左键拖动 / Alt+方向键\n缩放：滚轮 / Ctrl++、Ctrl+- · Ctrl+0 适合窗口"), previewPanel);
    navigationHelp->setWordWrap(true);
    previewLayout->addWidget(navigationHelp);
    resultStatus_ = new QLabel(previewPanel);
    resultStatus_->setObjectName("resultStatus");
    resultStatus_->setWordWrap(true);
    previewLayout->addWidget(resultStatus_);
    return previewPanel;
}

QWidget* MainWindow::createParametersPanel() {
    parameterPanel_ = new QWidget(this);
    parameterPanel_->setObjectName("parametersPanel");
    auto* panelLayout = new QVBoxLayout(parameterPanel_);
    partTitle_ = new QLabel(parameterPanel_);
    partTitle_->setObjectName("partTitle");
    partTitle_->setFont(sectionHeadingFont(this));
    panelLayout->addWidget(partTitle_);
    auto* inputHelp = new QLabel(QStringLiteral("单位：mm。输入后按 Enter 或移出输入框应用；清空可移除参数。"), parameterPanel_);
    inputHelp->setWordWrap(true);
    panelLayout->addWidget(inputHelp);
    form_ = new QFormLayout();
    form_->setRowWrapPolicy(QFormLayout::WrapLongRows);
    form_->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    form_->setVerticalSpacing(12);
    panelLayout->addLayout(form_);
    panelLayout->addStretch();
    auto* parameterScroll = new QScrollArea(this);
    parameterScroll->setWidgetResizable(true);
    parameterScroll->setFrameShape(QFrame::NoFrame);
    parameterScroll->setWidget(parameterPanel_);
    parameterScroll->setMinimumWidth(280);
    return parameterScroll;
}

QWidget* MainWindow::createComponentsPanel() {
    auto* componentsPanel = new QWidget(this);
    componentsPanel->setObjectName("componentsPanel");
    auto* componentsLayout = new QVBoxLayout(componentsPanel);
    auto* componentsTitle = new QLabel(QStringLiteral("组件树"), componentsPanel);
    componentsTitle->setFont(sectionHeadingFont(this));
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
    return componentsPanel;
}

} // namespace smarttank
