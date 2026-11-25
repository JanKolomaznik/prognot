#include <prognot/Qt/StackedProgressBar.hpp>
#include <QEnterEvent>
#include <QResizeEvent>
namespace prognot::qt {
ProgressBarStack::ProgressBarStack(QWidget* parent) : QWidget(parent), layout_(new QVBoxLayout(this)) {
    setWindowFlags(Qt::Popup | Qt::FramelessWindowHint);
}
QtProgressNotifier* ProgressBarStack::createProgressNotifier() {
    auto* bar = new ProgressBar(this);
    layout_->addWidget(bar);
    return bar->progressNotifier();
}
void ProgressBarStack::clear() {
    while (auto* item = layout_->takeAt(0)) { delete item->widget(); delete item; }
}
StackedProgressBar::StackedProgressBar(QWidget* parent) : QWidget(parent),
    bar_(new QProgressBar(this)), stack_(new ProgressBarStack(this)) {
    auto* layout = new QVBoxLayout(this);
    bar_->setRange(0, 100); bar_->setValue(0);
    layout->addWidget(bar_);
    connect(bar_, &QProgressBar::valueChanged, this, &StackedProgressBar::valueChanged);
    stack_->hide();
}
QtProgressNotifier* StackedProgressBar::createProgressNotifier(std::int64_t weight) {
    if (weight <= 0) throw std::invalid_argument("Task weight must be positive");
    auto* notifier = stack_->createProgressNotifier();
    const auto index = tasks_.size();
    tasks_.push_back({notifier, weight, Progress(0)});
    connect(notifier, &QtProgressNotifier::progressChanged, this, [this, index](Progress value) {
        tasks_[index].progress = std::move(value);
        updateProgress();
    });
    updateProgress();
    return notifier;
}
ProgressNotifier StackedProgressBar::makeNotifier(std::int64_t weight) {
    return createProgressNotifier(weight)->makeNotifier();
}
Progress StackedProgressBar::progress() const {
    Progress sum(0);
    boost::multiprecision::cpp_int weights = 0;
    for (const auto& task : tasks_) { sum += task.progress * task.weight; weights += task.weight; }
    return weights == 0 ? Progress(0) : sum / weights;
}
int StackedProgressBar::value() const noexcept { return bar_->value(); }
void StackedProgressBar::updateProgress() {
    const auto total = progress();
    bar_->setValue(((total.numerator() * 100) / total.denominator()).convert_to<int>());
}
void StackedProgressBar::cancel() { for (auto& task : tasks_) task.notifier->cancel(); }
void StackedProgressBar::clear() { tasks_.clear(); stack_->clear(); bar_->setValue(0); }
void StackedProgressBar::showSubtaskWindow(bool show) {
    if (show) {
        stack_->adjustSize();
        stack_->move(bar_->mapToGlobal(QPoint(0, -stack_->height())));
    }
    stack_->setVisible(show);
}
void StackedProgressBar::enterEvent(QEnterEvent* event) { showSubtaskWindow(); QWidget::enterEvent(event); }
void StackedProgressBar::resizeEvent(QResizeEvent* event) {
    stack_->setMinimumWidth(event->size().width()); QWidget::resizeEvent(event);
}
} // namespace prognot::qt
