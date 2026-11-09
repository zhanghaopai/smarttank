#include "ui/MainWindow.h"
#include "ui/ParameterEditor.h"
#include "view/OcctView.h"
#include "geometry/EnvelopeGeometry.h"
#include <QTreeWidget>
#include <QFormLayout>
#include <QLineEdit>
#include <QLabel>
#include <QFont>
#include <QStatusBar>
#include <QMessageBox>
#include <QCloseEvent>
#include <vector>
#include <Standard_Failure.hxx>

namespace smarttank {
namespace {
struct ParameterField { const char* key; QString label; };
std::vector<ParameterField> parameterFields(Part part) {
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

QString resultStateName(const ResultStamp& stamp) {
    switch (stamp.state) {
    case ResultState::Missing: return QStringLiteral("未生成");
    case ResultState::Current: return QStringLiteral("当前结果");
    case ResultState::Stale: return QStringLiteral("需重新生成");
    case ResultState::Invalid: return QStringLiteral("不可用");
    }
    return {};
}
}

QString MainWindow::partName(Part part) {
    switch (part) {
    case Part::Bottom: return QStringLiteral("罐底");
    case Part::Shell: return QStringLiteral("罐壁");
    case Part::Roof: return QStringLiteral("自支撑锥顶");
    }
    return {};
}

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setObjectName("mainWindow");
    setWindowTitle(QStringLiteral("SmartTank[*] — 自支撑锥顶储罐"));
    auto bodyFont = font();
    if (bodyFont.pointSizeF() < 12.0) bodyFont.setPointSizeF(12.0);
    setFont(bodyFont);
    resize(1280, 820);

    createWorkspace();
    createActions();
    connectWorkspaceSignals();

    tree_->setCurrentItem(tree_->topLevelItem(0)->child(1));
    updatePreview();
    refreshStatus();
}

void MainWindow::connectWorkspaceSignals() {
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
        previewErrorLabel_->setText(QStringLiteral("三维视图初始化失败：") + reason);
        previewErrorLabel_->show();
        statusBar()->showMessage(QStringLiteral("三维视图不可用"));
    });
}

void MainWindow::selectPart(Part part) {
    selected_ = part;
    partTitle_->setText(partName(part));
    while (form_->rowCount() > 0) form_->removeRow(0);
    for (const auto& field : parameterFields(part)) {
        addParameterField(part, field.key, field.label);
    }
    view_->highlightPart(part);
    refreshStatus();
}

void MainWindow::addParameterField(Part part, const char* key, const QString& fieldLabel) {
    const auto& values = project_.parameters(part);
    const auto found = values.find(key);
    const auto value = found == values.end() ? std::nullopt : std::optional<double>(found->second);
    auto* editor = new ParameterEditor(QString::fromLatin1(key), fieldLabel, value, parameterPanel_);
    auto* label = new QLabel(fieldLabel, parameterPanel_);
    label->setBuddy(editor->input());
    form_->addRow(label, editor);
    connect(editor, &ParameterEditor::valueApplied, this, [this, part, key = std::string(key)](std::optional<double> value) {
        if (!project_.setParameter(part, key, value)) return;
        setWindowModified(true);
        updatePreview();
        refreshStatus();
    });
}

void MainWindow::refreshStatus() {
    if (view_->failure().isEmpty()) {
        previewErrorLabel_->setText(previewError_);
        previewErrorLabel_->setVisible(!previewError_.isEmpty());
    }
    resultStatus_->setText(QStringLiteral("排板：%1    校核：%2\n基础校核不包含风、地震、锚固等专项。")
        .arg(resultStateName(project_.layoutStamp()), resultStateName(project_.calculationStamp())));
    statusBar()->showMessage(QStringLiteral("工程修订 %1 · %2").arg(project_.revision()).arg(partName(selected_)));
}

void MainWindow::updatePreview() {
    previewError_.clear();
    try {
        const auto dimensions = envelopeDimensions(project_);
        if (!dimensions) {
            view_->clearPlates();
            return;
        }
        view_->showEnvelope(*dimensions);
    } catch (const Standard_Failure& error) {
        showPreviewFailure(QString::fromUtf8(error.what()));
    } catch (const std::exception& error) {
        showPreviewFailure(QString::fromUtf8(error.what()));
    }
}

void MainWindow::showPreviewFailure(const QString& reason) {
    view_->clearPlates();
    previewError_ = QStringLiteral("无法生成外形预览：") + reason;
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
    if (!confirmDiscardChanges(QStringLiteral("新建工程"),
        QStringLiteral("当前参数尚未保存，是否清空并新建工程？"))) return;
    project_ = Project{};
    updatePreview();
    setWindowModified(false);
    selectPart(selected_);
    refreshStatus();
}

bool MainWindow::hasUnsavedChanges() const {
    if (isWindowModified()) return true;
    for (const auto* editor : parameterPanel_->findChildren<ParameterEditor*>()) {
        if (editor->hasUnappliedInput()) return true;
    }
    return false;
}

bool MainWindow::confirmDiscardChanges(const QString& title, const QString& message) {
    if (!hasUnsavedChanges()) return true;
    return QMessageBox::question(this, title, message, QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No) == QMessageBox::Yes;
}

void MainWindow::closeEvent(QCloseEvent* event) {
    if (!confirmDiscardChanges(QStringLiteral("退出 SmartTank"),
        QStringLiteral("当前参数尚未保存，是否丢弃并退出？"))) {
        event->ignore();
        return;
    }
    event->accept();
}

void MainWindow::loadPreviewExample() {
    if (!confirmDiscardChanges(QStringLiteral("载入示例尺寸"),
        QStringLiteral("是否用示例尺寸替换当前直径、高度和锥顶升高？其余参数保持不变。"))) return;
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
