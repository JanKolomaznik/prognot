#pragma once
/** @file ProgressDialog.hpp @brief Cancelable operation dialog for Qt 6. */
#include <QDialog>
#include <prognot/Qt/ProgressBar.hpp>
class QLabel;
namespace prognot::qt {
/**
 * @brief Single-operation dialog; accepts only successful operations.
 * All other outcomes reject it. Cancel, Escape, and closing request cooperative
 * cancellation and reject immediately; workers are never synchronously awaited.
 */
class ProgressDialog : public QDialog {
    Q_OBJECT
public:
    /** @brief Construct a dialog. @param parent Widget owner. */
    explicit ProgressDialog(QWidget* parent = nullptr);
    /** @brief Change operation description. @param text Displayed description. */
    void setText(const QString& text);
    /** @return Borrowed adapter, valid until dialog destruction. */
    QtProgressNotifier* progressNotifier() const noexcept;
    /** @return Owned core handle. @throws std::logic_error If already started. */
    [[nodiscard]] ProgressNotifier makeNotifier();
public slots:
    /** @brief Request cancellation and reject; also handles Escape and window close. */
    void reject() override;
private:
    bool rejected_ = false;
    QLabel* label_;
    ProgressBar* bar_;
};
} // namespace prognot::qt
