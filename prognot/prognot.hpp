#pragma once

#include <cstdint>
#include <exception>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <stop_token>
#include <utility>

#include <boost/iterator/iterator_adaptor.hpp>
#include <boost/multiprecision/cpp_int.hpp>
#include <boost/rational.hpp>

namespace prognot {

// Exact arithmetic, including deeply nested weighted tasks.
using Progress = boost::rational<boost::multiprecision::cpp_int>;

enum class Completion { succeeded, canceled, failed, abandoned };

struct CancelException : std::exception {
    const char* what() const noexcept override { return "Progress operation canceled"; }
};

class AProgressNotifierImpl {
public:
    virtual ~AProgressNotifierImpl() = default;
    virtual bool onIncrement(Progress progress) = 0;
    virtual void onStarted() {}
    virtual void onFinished() {}
    virtual void onStopped(Completion) {}
};

class DummyProgressNotifier : public AProgressNotifierImpl {
public:
    bool onIncrement(Progress) override { return true; }
};

class ProgressNotifier {
    struct State {
        explicit State(std::shared_ptr<AProgressNotifierImpl> sink) : sink(std::move(sink)) {
            if (!this->sink) throw std::invalid_argument("Null progress implementation");
            this->sink->onStarted();
        }
        ~State() noexcept {
            const auto result = stop.stop_requested() ? Completion::canceled
                : failed ? Completion::failed
                : progress == Progress(1) ? Completion::succeeded : Completion::abandoned;
            try { sink->onStopped(result); } catch (...) {}
            try { sink->onFinished(); } catch (...) {}
        }
        // Recursive only to detect callback reentry without deadlocking.
        std::recursive_mutex mutex;
        bool delivering = false;
        bool failed = false;
        Progress progress{0};
        std::stop_source stop;
        std::shared_ptr<AProgressNotifierImpl> sink;
    };

public:
    using StepCount = std::int64_t;

    explicit ProgressNotifier(std::shared_ptr<AProgressNotifierImpl> implementation)
        : state_(std::make_shared<State>(std::move(implementation))) {}

    // Borrowed implementation must outlive every notifier sharing this operation.
    explicit ProgressNotifier(AProgressNotifierImpl* implementation)
        : ProgressNotifier(borrow(implementation)) {}

    explicit ProgressNotifier(DummyProgressNotifier = {})
        : ProgressNotifier(std::make_shared<DummyProgressNotifier>()) {}

    ProgressNotifier(const ProgressNotifier&) = delete;
    ProgressNotifier& operator=(const ProgressNotifier&) = delete;
    ProgressNotifier(ProgressNotifier&& other) noexcept
        : state_(std::move(other.state_)), range_(std::move(other.range_)),
          step_(std::move(other.step_)), steps_(other.steps_), assigned_(other.assigned_),
          exceptions_(other.exceptions_), closed_(other.closed_) {}

    ProgressNotifier& operator=(ProgressNotifier&& other) noexcept {
        if (this != &other) {
            close();
            state_ = std::move(other.state_);
            range_ = std::move(other.range_);
            step_ = std::move(other.step_);
            steps_ = other.steps_;
            assigned_ = other.assigned_;
            exceptions_ = other.exceptions_;
            closed_ = other.closed_;
        }
        return *this;
    }

    ~ProgressNotifier() noexcept { close(); }

    void setStepCount(StepCount count) {
        requireOpen();
        if (count <= 0) throw std::invalid_argument("Step count must be positive");
        if (assigned_ != 0) throw std::logic_error("Cannot reset an active task");
        step_ = range_ / count;
        steps_ = count;
    }

    void increment(StepCount count = 1) {
        validate(count);
        auto state = state_;
        std::lock_guard lock(state->mutex);
        if (state->delivering) throw std::logic_error("Progress callback reentry");
        checkCancellation();
        Progress next = state->progress + step_ * count;
        state->progress = std::move(next);
        assigned_ += count;
        state->delivering = true;
        bool keepGoing;
        try {
            keepGoing = state->sink->onIncrement(state->progress);
        } catch (...) {
            state->delivering = false;
            state->failed = true;
            throw;
        }
        state->delivering = false;
        if (!keepGoing) state->stop.request_stop();
        checkCancellation();
    }

    [[nodiscard]] ProgressNotifier subTaskNotifier(StepCount count = 1) {
        validate(count);
        checkCancellation();
        ProgressNotifier child(state_, step_ * count);
        assigned_ += count;
        return child;
    }

    // Complete only this task's unallocated budget, never its children's budget.
    void finish() {
        requireOpen();
        if (steps_ == 0) throw std::logic_error("Set step count before finishing");
        checkCancellation();
        if (assigned_ < steps_) increment(steps_ - assigned_);
        closed_ = true;
    }

    void requestCancel() { requireState(); state_->stop.request_stop(); }
    void cancel() { requestCancel(); checkCancellation(); }
    void checkCancellation() const {
        requireState();
        if (state_->stop.stop_requested()) throw CancelException();
    }
    // Compatibility with the original misspelling.
    void checkCancelation() const { checkCancellation(); }
    [[nodiscard]] std::stop_token stopToken() const { requireState(); return state_->stop.get_token(); }
    [[nodiscard]] std::stop_source stopSource() const { requireState(); return state_->stop; }
    [[nodiscard]] Progress progress() const {
        requireState();
        std::lock_guard lock(state_->mutex);
        return state_->progress;
    }

private:
    ProgressNotifier(std::shared_ptr<State> state, Progress range)
        : state_(std::move(state)), range_(std::move(range)) {}

    static std::shared_ptr<AProgressNotifierImpl> borrow(AProgressNotifierImpl* implementation) {
        if (!implementation) throw std::invalid_argument("Null progress implementation");
        return {implementation, [](AProgressNotifierImpl*) {}};
    }
    void requireState() const {
        if (!state_) throw std::logic_error("Moved-from progress notifier");
    }
    void requireOpen() const {
        requireState();
        if (closed_) throw std::logic_error("Task already finished");
        std::lock_guard lock(state_->mutex);
        if (state_->delivering) throw std::logic_error("Progress callback reentry");
        if (state_->failed) throw std::logic_error("Progress operation failed");
    }
    void validate(StepCount count) const {
        requireOpen();
        if (count <= 0) throw std::invalid_argument("Increment must be positive");
        if (steps_ == 0) throw std::logic_error("Set step count first");
        if (count > steps_ - assigned_) throw std::out_of_range("Task budget exceeded");
    }
    void close() noexcept {
        if (!state_) return;
        if (std::uncaught_exceptions() > exceptions_) {
            std::lock_guard lock(state_->mutex);
            state_->failed = true;
        } else if (!closed_ && steps_ != 0) {
            try { finish(); } catch (...) {}
        }
        state_.reset();
    }

    std::shared_ptr<State> state_;
    Progress range_{1};
    Progress step_{0};
    StepCount steps_ = 0;
    StepCount assigned_ = 0;
    int exceptions_ = std::uncaught_exceptions();
    bool closed_ = false;
};

// Copies share a single traversal position: this is a single-pass adaptor.
template<typename TIterator>
class InputIteratorProgressNotifierAdaptor : public boost::iterator_adaptor<
    InputIteratorProgressNotifierAdaptor<TIterator>, TIterator,
    boost::use_default, boost::single_pass_traversal_tag> {
    using Base = boost::iterator_adaptor<InputIteratorProgressNotifierAdaptor<TIterator>,
        TIterator, boost::use_default, boost::single_pass_traversal_tag>;
    struct Traversal {
        Traversal(ProgressNotifier notifier, std::int64_t size)
            : notifier(std::move(notifier)), size(size) {
            if (size < 0) throw std::invalid_argument("Negative range size");
            this->notifier.setStepCount(size == 0 ? 1 : size);
            if (size == 0) this->notifier.finish();
        }
        ProgressNotifier notifier;
        std::int64_t size;
        std::int64_t position = 0;
    };
public:
    InputIteratorProgressNotifierAdaptor() = default;
    explicit InputIteratorProgressNotifierAdaptor(TIterator it, ProgressNotifier notifier,
                                                  std::int64_t size)
        : Base(std::move(it)), traversal_(std::make_shared<Traversal>(std::move(notifier), size)) {}
private:
    friend class boost::iterator_core_access;
    void increment() {
        if (!traversal_ || traversal_->position >= traversal_->size)
            throw std::out_of_range("Iterator progress range exhausted");
        traversal_->notifier.checkCancellation();
        ++this->base_reference();
        ++traversal_->position;
        traversal_->notifier.increment();
    }
    std::shared_ptr<Traversal> traversal_;
};

} // namespace prognot
