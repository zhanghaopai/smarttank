#include "ui/ParameterEditor.h"

#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QVBoxLayout>
#include <cmath>
#include <limits>

namespace smarttank {

ParameterEditor::ParameterEditor(const QString& key, const QString& label,
                                 std::optional<double> value, QWidget* parent)
    : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    input_ = new QLineEdit(this);
    input_->setObjectName(key);
    input_->setAccessibleName(label);
    input_->setPlaceholderText(QStringLiteral("未输入"));
    input_->setMinimumHeight(fontMetrics().height() + 12);
    input_->setClearButtonEnabled(true);
    if (value) {
        input_->setText(locale().toString(*value, 'g', std::numeric_limits<double>::max_digits10));
    }
    error_ = new QLabel(this);
    error_->setWordWrap(true);
    error_->hide();
    layout->addWidget(input_);
    layout->addWidget(error_);
    connect(input_, &QLineEdit::editingFinished, this, &ParameterEditor::applyInput);
}

bool ParameterEditor::hasUnappliedInput() const {
    return input_->isModified();
}

void ParameterEditor::applyInput() {
    const auto text = input_->text().trimmed();
    if (text.isEmpty()) {
        acceptValue(std::nullopt);
        return;
    }

    bool parsed = false;
    const auto value = locale().toDouble(text, &parsed);
    if (!parsed || !std::isfinite(value) || value <= 0) {
        error_->setText(QStringLiteral("输入错误：请输入大于零的有效数值，或清空。本次输入尚未应用。"));
        error_->show();
        input_->setAccessibleDescription(error_->text());
        return;
    }
    acceptValue(value);
}

void ParameterEditor::acceptValue(std::optional<double> value) {
    error_->hide();
    input_->setAccessibleDescription({});
    input_->setModified(false);
    emit valueApplied(value);
}

} // namespace smarttank
