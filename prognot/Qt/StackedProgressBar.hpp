#pragma once
/** @file StackedProgressBar.hpp @brief Weighted aggregation of independent operations. */
#include <QVBoxLayout>
#include <prognot/Qt/ProgressBar.hpp>
#include <vector>
namespace prognot::qt {
/** @brief Popup container for individual progress bars; GUI-thread API. */
class ProgressBarStack : public QWidget {
public:
    /** @brief Construct an empty popup. @param parent Widget owner. */
    explicit ProgressBarStack(QWidget* parent = nullptr);
    /** @return Adapter of a newly owned bar, initially zero. */
    QtProgressNotifier* createProgressNotifier();
    /** @brief Remove all bars, canceling any associated work. */
    void clear();
private:
    QVBoxLayout* layout_;
};
/**
 * @brief Weighted mean of independent core operations, with a popup per-task view.
 * GUI-thread API. Register all tasks before reporting for a fixed denominator.
 * Adding tasks later may lower the aggregate. Finished/failed/canceled tasks keep
 * their last exact fraction and weight until clear(); they are not treated as success.
 */
class StackedProgressBar : public QWidget {
    Q_OBJECT
public:
    /** @brief Construct an empty aggregate at zero. @param parent Widget owner. */
    explicit StackedProgressBar(QWidget* parent = nullptr);
    /**
     * @brief Add an independent operation with a positive relative weight.
     * @param weight Relative weight, default one (equal weighting).
     * @return Borrowed single-operation adapter, owned until clear() or destruction.
     * @throws std::invalid_argument If weight is not positive.
     */
    QtProgressNotifier* createProgressNotifier(std::int64_t weight = 1);
    /**
     * @brief Add an operation and create its core handle.
     * @param weight Positive relative weight.
     * @return Owned, unconfigured core handle.
     * @throws std::invalid_argument If weight is not positive.
     */
    [[nodiscard]] ProgressNotifier makeNotifier(std::int64_t weight = 1);
    /** @return Exact weighted mean, or zero for an empty group. */
    Progress progress() const;
    /** @return Aggregate percentage rounded down to [0,100]. */
    int value() const noexcept;
public slots:
    /** @brief Recompute aggregate from the latest delivered exact fractions. */
    void updateProgress();
    /** @brief Toggle per-task popup. @param show Whether to show it. */
    void showSubtaskWindow(bool show = true);
    /** @brief Request cancellation of all registered operations. */
    void cancel();
    /** @brief Cancel work, discard bars and reset aggregate to zero. */
    void clear();
signals:
    /** @brief Aggregate percentage changed. @param percentage Rounded-down percentage. */
    void valueChanged(int percentage);
protected:
    /** @brief Show details on pointer entry. @param event Qt 6 pointer-entry event. */
    void enterEvent(QEnterEvent* event) override;
    /** @brief Resize the details popup. @param event Widget resize event. */
    void resizeEvent(QResizeEvent* event) override;
private:
    struct Task { QtProgressNotifier* notifier; std::int64_t weight; Progress progress{0}; };
    std::vector<Task> tasks_;
    QProgressBar* bar_;
    ProgressBarStack* stack_;
};
} // namespace prognot::qt
