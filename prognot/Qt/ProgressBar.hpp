#pragma once
/** @file ProgressBar.hpp @brief Qt progress bar bound to one core operation. */
#include <QProgressBar>
#include <prognot/Qt/ProgressNotifier.hpp>
namespace prognot::qt {
/** @brief Single-operation progress bar, initially zero; GUI-thread API. */
class ProgressBar : public QProgressBar {
public:
    /** @brief Construct a bar and its adapter. @param parent Widget owner. */
    explicit ProgressBar(QWidget* parent = nullptr) : QProgressBar(parent) {
        setRange(0, 100);
        setValue(0);
        QObject::connect(&notifier_, &QtProgressNotifier::incremented, this, &QProgressBar::setValue);
    }
    /** @return Borrowed adapter, valid until this bar is destroyed. */
    QtProgressNotifier* progressNotifier() noexcept { return &notifier_; }
    /** @return Owned core handle. @throws std::logic_error If already started. */
    [[nodiscard]] ProgressNotifier makeNotifier() { return notifier_.makeNotifier(); }
private:
    QtProgressNotifier notifier_;
};
} // namespace prognot::qt
