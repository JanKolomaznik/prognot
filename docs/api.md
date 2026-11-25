# Core API contract (0.2)

## Values and budgets

`Progress` is `boost::rational<boost::multiprecision::cpp_int>`. Values are exact
fractions of the entire operation, from zero to one. Arbitrary precision avoids
fixed-width overflow in deeply nested tasks; arithmetic cost grows with fraction
size. Use `boost::rational_cast<double>(value)` for display conversion.

`ProgressNotifier` is move-only. A root owns the full range `[0, 1]`; children
share the root's state. Each handle must be configured with `setStepCount(n)`
before incrementing, finishing, or allocating children. Counts are signed 64-bit
integers and must be positive. Reconfiguration is permitted only before any
steps have been consumed or allocated.

- `increment(n = 1)` consumes and reports exactly `n` local steps.
- `subTaskNotifier(n = 1)` reserves `n` local steps and returns an unconfigured
  child representing that fraction. Reservation itself reports no progress.
- `finish()` reports all remaining **unallocated** local steps and closes this
  handle. It does not finish children or wait for them. Repeated finishing or
  subsequent modification throws `std::logic_error`.
- `progress()` returns a synchronized snapshot of the whole operation.

For example, a three-step child of a ten-step root owns 30% of the whole
operation, independent of how many steps that child chooses internally. Children
can outlive their parent. The total only reaches 100% when all reserved ranges
have actually been accounted for.

Invalid/nonpositive counts throw `std::invalid_argument`; exceeding the remaining
budget throws `std::out_of_range`; unconfigured, finished, failed, or moved-from
handles reject modifications with `std::logic_error`. Validation is active in
release builds. Rejected count/budget changes do not consume progress.

## Ownership and lifetime

Prefer construction from `std::shared_ptr<AProgressNotifierImpl>`. The operation
keeps the implementation alive until its last handle is destroyed. Construction
from a raw pointer remains available as a **borrowed** interface: the caller must
keep that object alive until every parent, child, and iterator copy is destroyed.
Null implementations are rejected. Default construction, or construction from
`DummyProgressNotifier{}`, creates an owned no-op implementation.

The virtual base destructor permits safe polymorphic ownership. Moving transfers
the handle; moved-from handles may only be destroyed or assigned to. Move
assignment first closes the old task using the same rules as destruction.
Self-move assignment leaves the task unchanged.

## Concept-constrained callbacks

For progress-only reporting, `makeProgressNotifier(callback)` owns a copy of an
lvalue callback or moves an rvalue callback into the shared operation state:

```cpp
auto notifier = prognot::makeProgressNotifier([](const prognot::Progress& value) {
    // Display or record value.
    return true;
});
notifier.setStepCount(10);
notifier.increment();
notifier.finish();
```

The public `ProgressCallback<T>` concept requires a callable invocable as `T&`
with a `Progress` argument and returning exactly `bool`. A `void` or integer
return is rejected at compile time, avoiding ambiguous cancellation semantics.
The factory additionally requires that its decayed callback type can be
constructed from the supplied argument. Mutable and move-only lambdas and
function pointers are supported; a move-only lvalue must be explicitly moved.
The callable is owned, but references captured by a lambda remain borrowed.

Callback return values, exceptions, serialization, and threading follow the same
rules as `onIncrement()`. Use `AProgressNotifierImpl` when lifecycle callbacks are
needed. Concepts constrain type-level requirements only; positive step counts,
remaining budgets, and lifetime rules still require runtime validation.

## Scope exit and operation outcome

Normal destruction of a configured open handle calls `finish()`. An unconfigured
handle contributes nothing, including an unconfigured child. When destruction
occurs during exception unwinding, the operation is marked failed and that
handle's remainder is not filled. Once failed, all handles reject modifications.
Exceptions from progress callbacks also mark the operation failed. Destructors
are `noexcept` and suppress all callback exceptions; they never log implicitly.

When the last handle disappears, the implementation receives:

1. `onStopped(Completion)` once.
2. `onFinished()` once, even if `onStopped()` throws.

The result is `canceled` if cancellation was requested, otherwise `failed` if
unwinding or a progress callback failed, otherwise `succeeded` if progress equals
one, otherwise `abandoned`. Thus cancellation takes precedence over failure.
`onFinished()` means lifecycle end, **not success**. An unconfigured root or an
unconfigured reserved child can lead to `abandoned`.

`onStarted()` is called synchronously during root construction, exactly once.
If it throws, construction fails and there are no terminal callbacks. Terminal
callbacks run on the thread releasing the last handle and their exceptions are
suppressed. A retained stop token/source does not extend the operation lifetime.

## Cancellation

Cancellation is cooperative and shared by all children:

- `requestCancel()` requests cancellation without throwing.
- `cancel()` requests cancellation and throws `CancelException`.
- `checkCancellation()` throws if cancellation was requested. The original
  misspelled `checkCancelation()` remains an alias.
- `stopToken()` exposes a C++20 `std::stop_token` for polling or callbacks.
- `stopSource()` returns a shared cancellation source suitable for external
  threads. `source.request_stop()` does not throw.
- Returning `false` from `onIncrement()` requests cancellation; that increment
  then throws `CancelException`. The just-reported progress remains committed.

Increment, child allocation, and explicit completion check cancellation.
Destruction does not fill canceled work. Cancellation does not interrupt blocking
I/O, stop a thread, or roll back an increment racing with a cancellation request.
Configuration itself does not poll cancellation. Callbacks that throw also leave
their reported progress committed; retries cannot replay those steps.

## Threads and callbacks

Use a separate handle per worker, usually one child per thread. The same handle
must not be concurrently modified, moved, or destroyed. Independent children may
report concurrently. Allocate those children serially from their parent first.
Snapshots and shared cancellation are synchronized, but the object whose method
is called must remain alive throughout the call.

Progress updates and `onIncrement()` calls are serialized by the shared state,
so callbacks receive monotonically increasing values in commit order. Callbacks
run synchronously on the reporting thread; this library does not dispatch to a
UI thread or throttle notifications. A slow callback blocks other reporters.

Callbacks may read progress or request cancellation. They must not modify task
budgets, finish tasks, allocate children, move/destroy handles, or synchronously
wait for another reporter. Reentrant modifications are rejected with
`std::logic_error`. Stop callbacks run synchronously according to the standard
stop-token rules and must likewise avoid waiting for reporting threads.

## Iterator adaptor

`InputIteratorProgressNotifierAdaptor<Iterator>(begin, notifier, size)` consumes
a notifier and configures it for exactly `size` increments. Each successful
iterator advance reports one element; a zero-length range completes immediately.
Negative sizes throw `std::invalid_argument`. Incrementing a default adaptor or
advancing beyond the declared size throws `std::out_of_range` before advancing
the underlying iterator. The supplied size must match the actual range; the
adaptor cannot discover an underlying end iterator.

This is a single-pass adaptor. Copies share progress and traversal accounting;
use one advancing copy at a time and do not traverse the same range independently
through copied adaptors. Copies retain the operation until the last copy is
released. Early ordinary destruction fills the unvisited remainder, following
the same scope-completion policy as other handles; it does not prove that every
element was processed. Cancellation or exception unwinding suppresses that fill.
Notifications are per element, without the previous fixed 100-step rounding.
The adaptor uses standard C++20 iterator concepts and traits, without Boost.Iterator.
It exposes dereference, pre/post increment, `base()`, and equality when the underlying
iterator supports it. `operator->` is available for lvalue references.

## Migration from the 2014 prototype

- Requires C++20, CMake 3.28+, and Boost 1.85+.
- The core target is header-only `prognot::prognot`; Qt is optional through
  `PROGNOT_BUILD_QT` and the `prognot::qt` target. See [Qt integration](qt.md).
- `Progress` no longer uses fixed-width `int`. Code inspecting numerator or
  denominator types may need changes; display conversions still use Rational.
- `CancelException` now derives only from `std::exception`, not Boost.Exception.
- Invalid budgets throw instead of relying on assertions. Active tasks cannot
  reset their step count. Callback failures stop further reporting.
- Destructors safely suppress every exception. Use explicit `finish()` when
  failures must propagate. Use `onStopped()` to distinguish operation outcomes.
- The old logging/assertion macros and the protected implementation details are
  removed. Public original method names are retained where practical.
