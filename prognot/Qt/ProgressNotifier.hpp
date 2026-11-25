#pragma once
/** @file ProgressNotifier.hpp @brief Safe Qt 6 delivery of core progress events. */
#include <QObject>
#include <prognot/prognot.hpp>

Q_DECLARE_METATYPE(prognot::Progress)
Q_DECLARE_METATYPE(prognot::Completion)

/** @brief Qt 6 adapters and widgets; widget APIs run on the GUI thread. */
namespace prognot::qt {
/**
 * @brief Single-operation GUI observer with lifetime-safe queued delivery.
 * Create and destroy on its QObject thread. All public methods are GUI-thread
 * only. Workers use the returned core handle, never this object. Destruction
 * requests cancellation; surviving workers may safely poll or release handles.
 * One operation per adapter prevents stale queued events affecting a new run.
 */
class QtProgressNotifier : public QObject {
    Q_OBJECT
public:
    /** @brief Construct an unused adapter. @param parent QObject owner. */
    explicit QtProgressNotifier(QObject* parent = nullptr);
    /** @brief Cancel work and detach queued delivery without waiting for workers. */
    ~QtProgressNotifier() override;
    /**
     * @return An owned, unconfigured core root handle, suitable for moving to a worker.
     * @throws std::logic_error If this adapter has already created an operation.
     */
    [[nodiscard]] ProgressNotifier makeNotifier();
    /** @return Whether work started and its terminal event has not yet been delivered. */
    bool isRunning() const noexcept;
public slots:
    /** @brief Request cancellation without throwing; harmless before starting. */
    void cancel();
signals:
    /** @brief Exact committed progress delivered on the GUI thread. @param value Whole-operation fraction. */
    void progressChanged(prognot::Progress value);
    /** @brief Rounded-down display percentage. @param percentage Integer in [0,100]. */
    void incremented(int percentage);
    /** @brief Operation started; emitted asynchronously. */
    void started();
    /** @brief Final result, followed by finished() if this object survives. @param result Core operation outcome. */
    void stopped(prognot::Completion result);
    /** @brief Operation ended; this does not imply success. */
    void finished();
private:
    struct Delivery;
    struct Bridge;
    std::shared_ptr<Delivery> delivery_;
    std::stop_source source_{std::nostopstate};
    bool used_ = false;
    bool running_ = false;
};
} // namespace prognot::qt
