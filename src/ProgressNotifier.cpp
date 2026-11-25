#include <prognot/Qt/ProgressNotifier.hpp>
#include <QMetaObject>
#include <QPointer>

namespace prognot::qt {
struct QtProgressNotifier::Delivery {
    std::mutex mutex;
    QtProgressNotifier* target;
    explicit Delivery(QtProgressNotifier* target) : target(target) {}
    template<class F> void post(F action) {
        std::lock_guard lock(mutex);
        if (target) {
            auto* receiver = target;
            QMetaObject::invokeMethod(receiver, [receiver, action = std::move(action)]() mutable {
                action(*receiver);
            }, Qt::QueuedConnection);
        }
    }
};
struct QtProgressNotifier::Bridge final : AProgressNotifierImpl {
    std::shared_ptr<Delivery> delivery;
    explicit Bridge(std::shared_ptr<Delivery> delivery) : delivery(std::move(delivery)) {}
    void onStarted() override {
        delivery->post([](QtProgressNotifier& q) { emit q.started(); });
    }
    bool onIncrement(Progress value) override {
        delivery->post([value = std::move(value)](QtProgressNotifier& q) {
            const int percentage = ((value.numerator() * 100) / value.denominator()).convert_to<int>();
            QPointer<QtProgressNotifier> guard(&q);
            emit q.progressChanged(value);
            if (guard) emit guard->incremented(percentage);
        });
        return true;
    }
    void onStopped(Completion result) override {
        delivery->post([result](QtProgressNotifier& q) {
            q.running_ = false;
            QPointer<QtProgressNotifier> guard(&q);
            emit q.stopped(result);
            if (guard) emit guard->finished();
        });
    }
};
QtProgressNotifier::QtProgressNotifier(QObject* parent)
    : QObject(parent), delivery_(std::make_shared<Delivery>(this)) {
    qRegisterMetaType<Progress>();
    qRegisterMetaType<Completion>();
}
QtProgressNotifier::~QtProgressNotifier() {
    source_.request_stop();
    std::lock_guard lock(delivery_->mutex);
    delivery_->target = nullptr;
}
ProgressNotifier QtProgressNotifier::makeNotifier() {
    if (used_) throw std::logic_error("Qt adapter already used; create a new adapter");
    ProgressNotifier handle(std::make_shared<Bridge>(delivery_));
    source_ = handle.stopSource();
    used_ = true;
    running_ = true;
    return handle;
}
bool QtProgressNotifier::isRunning() const noexcept { return running_; }
void QtProgressNotifier::cancel() { source_.request_stop(); }
} // namespace prognot::qt
