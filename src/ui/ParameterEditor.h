#pragma once

#include <QWidget>
#include <optional>

class QLineEdit;
class QLabel;

namespace smarttank {

// Edits a positive optional value; project state is owned by the caller.
class ParameterEditor : public QWidget {
    Q_OBJECT
public:
    ParameterEditor(const QString& key, const QString& label,
                    std::optional<double> value, QWidget* parent = nullptr);
    QLineEdit* input() const { return input_; }
    bool hasUnappliedInput() const;

signals:
    void valueApplied(std::optional<double> value);

private:
    void applyInput();
    void acceptValue(std::optional<double> value);

    QLineEdit* input_ = nullptr;
    QLabel* error_ = nullptr;
};

} // namespace smarttank
