/**
 * @file prognot.hpp
 * @brief C++20 weighted progress reporting and cooperative cancellation.
 */
#pragma once

#include <cstdint>
#include <exception>
#include <functional>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <stop_token>
#include <utility>

#include <concepts>
#include <iterator>
#include <type_traits>
#include <boost/multiprecision/cpp_int.hpp>
#include <boost/rational.hpp>

/** @brief UI-independent progress reporting primitives. */
namespace prognot {

/**
 * @brief Exact fraction of the complete operation, in [0, 1].
 * Arbitrary-precision integers prevent overflow in nested task weights.
 * Convert for display with boost::rational_cast<double>().
 */
using Progress = boost::rational<boost::multiprecision::cpp_int>;

/**
 * @brief Final operation outcome, delivered when the last task handle disappears.
 * Cancellation takes precedence over failure, then success, then abandonment.
 */
enum class Completion {
    succeeded, ///< Every reserved range has been reported; total progress is one.
    canceled,  ///< Cancellation was requested, even if all work was reported.
    failed,    ///< Exception unwinding or a progress callback marked the operation failed.
    abandoned  ///< Some work was never configured or reported.
};

/** @brief Exception signaling cooperative cancellation of an operation. */
struct CancelException : std::exception {
    /** @return A static, null-terminated cancellation description. */
    const char* what() const noexcept override { return "Progress operation canceled"; }
};

/**
 * @brief Polymorphic observer for progress and lifecycle events.
 * Progress callbacks are synchronous and serialized across an operation's tasks.
 * They may inspect progress or request cancellation, but must not mutate task
 * budgets, destroy/move handles, or wait for another reporting thread.
 * Terminal callbacks run on the thread releasing the last handle.
 */
class AProgressNotifierImpl {
public:
    /** @brief Allow safe destruction through the observer base class. */
    virtual ~AProgressNotifierImpl() = default;
    /**
     * @brief Observe a committed progress update.
     * @param progress Exact cumulative fraction of the whole operation.
     * @return True to continue; false to request cancellation and make the
     * reporting increment throw CancelException.
     * @note Exceptions propagate from explicit reporting, mark the operation
     * failed, and do not roll back the committed update.
     */
    virtual bool onIncrement(Progress progress) = 0;
    /**
     * @brief Called once synchronously during root construction.
     * @note An exception aborts construction; terminal callbacks are not called.
     */
    virtual void onStarted() {}
    /**
     * @brief Called once after onStopped(), including canceled/failed operations.
     * This signals lifecycle end, not success. Exceptions are suppressed.
     */
    virtual void onFinished() {}
    /**
     * @brief Receive the final outcome before onFinished().
     * @param result Outcome of the shared operation; exceptions are suppressed.
     */
    virtual void onStopped(Completion result) { (void)result; }
};

/** @brief No-op observer that always permits continued execution. */
class DummyProgressNotifier : public AProgressNotifierImpl {
public:
    /** @brief Ignore an update. @param progress Unused fraction. @return Always true. */
    bool onIncrement(Progress progress) override { (void)progress; return true; }
};

/**
 * @brief Move-only handle to a weighted task within a shared operation.
 * Configure a handle with setStepCount() before reporting or allocating children.
 * Children may outlive parents. Independent handles may report concurrently;
 * the same handle must not be concurrently modified, moved, or destroyed.
 * All methods require a live, non-moved-from handle unless stated otherwise.
 * @see makeProgressNotifier(), AProgressNotifierImpl
 */
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
    /** @brief Signed 64-bit local step budget; valid counts are positive. */
    using StepCount = std::int64_t;

    /**
     * @brief Create a root operation owning its observer.
     * @param implementation Observer retained until the last task is destroyed.
     * @throws std::invalid_argument If implementation is null.
     * @note Exceptions from onStarted() and allocation propagate.
     */
    explicit ProgressNotifier(std::shared_ptr<AProgressNotifierImpl> implementation)
        : state_(std::make_shared<State>(std::move(implementation))) {}

    /**
     * @brief Create a root operation borrowing an observer.
     * @param implementation Non-null observer which must outlive all shared tasks
     * and iterator copies. Ownership is not transferred.
     * @throws std::invalid_argument If implementation is null.
     * @note Exceptions from onStarted() and allocation propagate.
     */
    explicit ProgressNotifier(AProgressNotifierImpl* implementation)
        : ProgressNotifier(borrow(implementation)) {}

    /** @brief Create an operation with an owned no-op observer. */
    explicit ProgressNotifier(DummyProgressNotifier = {})
        : ProgressNotifier(std::make_shared<DummyProgressNotifier>()) {}

    /** @brief Copying task budgets is forbidden; create a subtask instead. */
    ProgressNotifier(const ProgressNotifier&) = delete;
    /** @brief Copy assignment is forbidden. */
    ProgressNotifier& operator=(const ProgressNotifier&) = delete;
    /**
     * @brief Transfer a task without reporting progress.
     * @param other Source handle, left valid only for destruction or assignment.
     */
    ProgressNotifier(ProgressNotifier&& other) noexcept
        : state_(std::move(other.state_)), range_(std::move(other.range_)),
          step_(std::move(other.step_)), steps_(other.steps_), assigned_(other.assigned_),
          exceptions_(other.exceptions_), closed_(other.closed_) {}

    /**
     * @brief Close the old task, then transfer another task into this handle.
     * @param other Source handle; self-move has no effect.
     * @return This handle.
     * @note Closing follows destructor rules and suppresses callback exceptions.
     */
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

    /**
     * @brief Close this task and release its share of the operation.
     * Normal scope exit completes configured, open, unallocated work. Unwinding
     * marks failure instead. Failed or canceled operations receive no synthetic
     * completion. Unconfigured tasks contribute nothing. All exceptions are
     * suppressed; use finish() to observe callback failures explicitly.
     */
    ~ProgressNotifier() noexcept { close(); }

    /**
     * @brief Divide this task's range into equal local steps.
     * @param count Positive local budget. May be reset before any allocation/update.
     * @throws std::invalid_argument If count is not positive.
     * @throws std::logic_error If work was assigned, the handle is closed/moved,
     * the operation failed, or this call reenters a progress callback.
     */
    void setStepCount(StepCount count) {
        requireOpen();
        if (count <= 0) throw std::invalid_argument("Step count must be positive");
        if (assigned_ != 0) throw std::logic_error("Cannot reset an active task");
        step_ = range_ / count;
        steps_ = count;
    }

    /**
     * @brief Consume local steps and synchronously report their full weight.
     * @param count Positive number of steps to consume.
     * @throws std::invalid_argument If count is not positive.
     * @throws std::out_of_range If count exceeds the unallocated budget.
     * @throws std::logic_error If unconfigured, closed, moved-from, failed, or reentrant.
     * @throws CancelException If cancellation is observed before or after reporting.
     * @note Callback exceptions propagate and mark failure. A reported update
     * remains committed on exception or cancellation. Notifications are serialized.
     */
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

    /**
     * @brief Reserve a weighted child without reporting progress.
     * @param count Positive number of this task's steps assigned to the child.
     * @return An unconfigured child; call setStepCount() before using it.
     * @throws std::invalid_argument If count is not positive.
     * @throws std::out_of_range If the remaining local budget is insufficient.
     * @throws std::logic_error If unconfigured, closed, moved-from, failed, or reentrant.
     * @throws CancelException If cancellation was requested.
     */
    [[nodiscard]] ProgressNotifier subTaskNotifier(StepCount count = 1) {
        validate(count);
        checkCancellation();
        ProgressNotifier child(state_, step_ * count);
        assigned_ += count;
        return child;
    }

    /**
     * @brief Report this task's unallocated remainder and close the handle.
     * Does not finish or wait for children. Repeated calls are rejected.
     * @throws std::logic_error If unconfigured, closed, moved-from, failed, or reentrant.
     * @throws CancelException If cancellation is observed.
     * @note Callback exceptions propagate. A closed handle can still inspect
     * shared progress and request cancellation.
     */
    void finish() {
        requireOpen();
        if (steps_ == 0) throw std::logic_error("Set step count before finishing");
        checkCancellation();
        if (assigned_ < steps_) increment(steps_ - assigned_);
        closed_ = true;
    }

    /**
     * @brief Request shared cancellation without throwing CancelException.
     * @throws std::logic_error If moved-from.
     * @note Stop callbacks may run synchronously. No thread is interrupted.
     */
    void requestCancel() { requireState(); state_->stop.request_stop(); }
    /**
     * @brief Request shared cancellation and throw CancelException.
     * @throws CancelException Always for a live handle.
     * @throws std::logic_error If moved-from.
     */
    void cancel() { requestCancel(); checkCancellation(); }
    /**
     * @brief Poll the shared cooperative cancellation state.
     * @throws CancelException If cancellation was requested.
     * @throws std::logic_error If moved-from.
     */
    void checkCancellation() const {
        requireState();
        if (state_->stop.stop_requested()) throw CancelException();
    }
    /** @brief Compatibility alias for checkCancellation(), with identical exceptions. */
    void checkCancelation() const { checkCancellation(); }
    /**
     * @return A shared cancellation token, independent of the task's lifetime.
     * @throws std::logic_error If moved-from.
     */
    [[nodiscard]] std::stop_token stopToken() const { requireState(); return state_->stop.get_token(); }
    /**
     * @return A shared source suitable for requesting cancellation externally.
     * Keeping this source does not retain the observer or operation state.
     * @throws std::logic_error If moved-from.
     */
    [[nodiscard]] std::stop_source stopSource() const { requireState(); return state_->stop; }
    /**
     * @return A synchronized snapshot of the whole operation, not just this task.
     * @throws std::logic_error If moved-from.
     */
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

/**
 * @brief Callable accepting Progress and returning exactly bool.
 * @tparam Callback Callable type invoked as an lvalue; mutable callables work.
 * Void and merely bool-convertible results are rejected to make cancellation
 * decisions explicit. This concept does not impose ownership or copyability.
 */
template<class Callback>
concept ProgressCallback = std::invocable<Callback&, Progress>
    && std::same_as<std::invoke_result_t<Callback&, Progress>, bool>;

/// @cond INTERNAL
namespace detail {
template<ProgressCallback Callback>
class CallbackSink final : public AProgressNotifierImpl {
public:
    template<class F> requires std::constructible_from<Callback, F&&>
    explicit CallbackSink(F&& callback) : callback_(std::forward<F>(callback)) {}

    bool onIncrement(Progress progress) override {
        return std::invoke(callback_, std::move(progress));
    }
private:
    Callback callback_;
};
} // namespace detail
/// @endcond

/**
 * @brief Create an operation owning a progress-only callback.
 * @tparam Callback Type satisfying ProgressCallback after decay and constructible
 * from the forwarded argument. Mutable and move-only callables are supported.
 * @param callback Copied from an lvalue or moved from an rvalue. References
 * captured by a lambda remain borrowed and must outlive all task handles.
 * @return An unconfigured root notifier; call setStepCount() before reporting.
 * @note Callable construction/allocation exceptions propagate. Callback return
 * values and exceptions follow AProgressNotifierImpl::onIncrement() rules.
 * Use the virtual observer interface when lifecycle notifications are required.
 */
template<class Callback>
    requires ProgressCallback<std::decay_t<Callback>>
        && std::constructible_from<std::decay_t<Callback>, Callback&&>
[[nodiscard]] ProgressNotifier makeProgressNotifier(Callback&& callback) {
    return ProgressNotifier(std::make_shared<detail::CallbackSink<std::decay_t<Callback>>>(
        std::forward<Callback>(callback)));
}

/**
 * @brief Single-pass input iterator reporting one step per element advanced.
 * @tparam TIterator A type satisfying std::input_iterator.
 * Copies share traversal accounting, but retain their own underlying iterator.
 * Advance only one copy per traversal; do not use copies for independent passes.
 * The declared size must match the actual range; no end iterator is stored.
 * Releasing the last copy closes the owned notifier, including scope completion
 * on early normal exit. This does not prove that every element was processed.
 * Concurrent access to shared traversal accounting requires external synchronization.
 */
template<std::input_iterator TIterator>
class InputIteratorProgressNotifierAdaptor {
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
    /** @brief C++20 input iterator category. */
    using iterator_concept = std::input_iterator_tag;
    /** @brief Legacy input iterator category. */
    using iterator_category = std::input_iterator_tag;
    /** @brief Underlying element value type. */
    using value_type = std::iter_value_t<TIterator>;
    /** @brief Underlying signed iterator distance type. */
    using difference_type = std::iter_difference_t<TIterator>;
    /** @brief Underlying dereference result, possibly a proxy. */
    using reference = std::iter_reference_t<TIterator>;
    /** @brief Pointer trait; operator-> is available only for lvalue references. */
    using pointer = std::add_pointer_t<reference>;

    /** @brief Create an unbound adaptor; incrementing it throws std::out_of_range. */
    InputIteratorProgressNotifierAdaptor() requires std::default_initializable<TIterator> = default;
    /**
     * @brief Bind traversal progress to an iterator and an owned task handle.
     * @param it Start iterator for the declared range.
     * @param notifier Task moved into shared traversal state; must permit configuration.
     * @param size Number of elements; zero finishes immediately without advancing.
     * @throws std::invalid_argument If size is negative.
     * @throws std::logic_error If notifier cannot be configured.
     * @note For zero size, exceptions from ProgressNotifier::finish() propagate.
     */
    explicit InputIteratorProgressNotifierAdaptor(TIterator it, ProgressNotifier notifier,
                                                  std::int64_t size)
        : iterator_(std::move(it)), traversal_(std::make_shared<Traversal>(std::move(notifier), size)) {}

    /** @return The stored underlying iterator, without advancing. */
    const TIterator& base() const noexcept { return iterator_; }
    /** @return The underlying element. @pre The underlying iterator is dereferenceable. */
    decltype(auto) operator*() const { return *iterator_; }
    /** @return Address of the element. @pre The underlying iterator is dereferenceable. */
    auto operator->() const requires std::is_lvalue_reference_v<reference> {
        return std::addressof(*iterator_);
    }
    /**
     * @brief Advance the underlying iterator and report one step.
     * @return This adaptor.
     * @throws std::out_of_range If unbound or the declared range is exhausted.
     * @throws CancelException If cancellation is observed.
     * @note Underlying iterator and reporting exceptions propagate. An exception
     * after advancing does not roll back the iterator or traversal accounting.
     */
    InputIteratorProgressNotifierAdaptor& operator++() {
        increment();
        return *this;
    }
    /**
     * @brief Advance a copyable iterator, returning its previous position.
     * @return A copy retaining shared traversal state.
     * @note Exceptions follow prefix increment and underlying copy construction.
     */
    InputIteratorProgressNotifierAdaptor operator++(int) requires std::copyable<TIterator> {
        auto previous = *this;
        ++*this;
        return previous;
    }
    /** @brief Advance a noncopyable iterator; exceptions follow prefix increment. */
    void operator++(int) requires (!std::copyable<TIterator>) { ++*this; }
    /**
     * @brief Compare underlying positions, ignoring traversal/progress state.
     * @param left First adaptor.
     * @param right Second adaptor.
     * @return Whether the underlying iterators compare equal.
     */
    friend bool operator==(const InputIteratorProgressNotifierAdaptor& left,
                           const InputIteratorProgressNotifierAdaptor& right)
        requires std::equality_comparable<TIterator> {
        return left.iterator_ == right.iterator_;
    }
private:
    void increment() {
        if (!traversal_ || traversal_->position >= traversal_->size)
            throw std::out_of_range("Iterator progress range exhausted");
        traversal_->notifier.checkCancellation();
        ++iterator_;
        ++traversal_->position;
        traversal_->notifier.increment();
    }
    TIterator iterator_{};
    std::shared_ptr<Traversal> traversal_;
};

} // namespace prognot
