#define BOOST_TEST_MODULE prognot_qt
#include <boost/test/included/unit_test.hpp>
#include <prognot/Qt/ProgressDialog.hpp>
#include <prognot/Qt/StackedProgressBar.hpp>
#include <QApplication>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QThread>
#include <thread>
using namespace prognot;
using namespace prognot::qt;
struct Application {
    int argc = 1;
    char name[12] = "prognot_qt";
    char* argv[2] = {name, nullptr};
    QApplication app{argc, argv};
};
BOOST_GLOBAL_FIXTURE(Application);
static void drain() {
    for (int i = 0; i < 5; ++i) {
        QCoreApplication::sendPostedEvents(nullptr, QEvent::MetaCall);
        QCoreApplication::processEvents();
    }
}
BOOST_AUTO_TEST_CASE(worker_delivery_and_lifecycle) {
    ProgressBar bar;
    std::vector<int> values;
    std::vector<Completion> outcomes;
    int starts = 0, finishes = 0;
    auto* adapter = bar.progressNotifier();
    QObject::connect(adapter, &QtProgressNotifier::started, [&] { ++starts; });
    QObject::connect(adapter, &QtProgressNotifier::incremented, [&](int value) {
        BOOST_CHECK(QThread::currentThread() == qApp->thread()); values.push_back(value);
    });
    QObject::connect(adapter, &QtProgressNotifier::stopped, [&](Completion result) { outcomes.push_back(result); });
    QObject::connect(adapter, &QtProgressNotifier::finished, [&] { ++finishes; });
    auto handle = bar.makeNotifier();
    BOOST_CHECK_THROW((void)bar.makeNotifier(), std::logic_error);
    std::jthread worker([handle = std::move(handle)]() mutable {
        handle.setStepCount(3); handle.increment(); handle.increment(); handle.finish();
    });
    worker.join();
    BOOST_CHECK(values.empty());
    BOOST_CHECK_EQUAL(bar.value(), 0);
    drain();
    BOOST_REQUIRE_EQUAL(values.size(), 3U);
    BOOST_CHECK_EQUAL(values[0], 33); BOOST_CHECK_EQUAL(values[1], 66); BOOST_CHECK_EQUAL(values[2], 100);
    BOOST_CHECK_EQUAL(bar.value(), 100);
    BOOST_CHECK_EQUAL(starts, 1); BOOST_CHECK_EQUAL(finishes, 1);
    BOOST_REQUIRE_EQUAL(outcomes.size(), 1U);
    BOOST_CHECK(outcomes[0] == Completion::succeeded);
    BOOST_CHECK(!adapter->isRunning());
}
BOOST_AUTO_TEST_CASE(exact_weighted_aggregation) {
    StackedProgressBar stack;
    BOOST_CHECK(stack.progress() == Progress(0));
    BOOST_CHECK_THROW((void)stack.makeNotifier(0), std::invalid_argument);
    {
        auto first = stack.makeNotifier(1);
        auto second = stack.makeNotifier(3);
        first.setStepCount(3); second.setStepCount(3);
        first.increment(); second.increment(); drain();
        BOOST_CHECK(stack.progress() == Progress(1,3)); // not rounded percentages
        BOOST_CHECK_EQUAL(stack.value(), 33);
        first.finish(); drain();
        BOOST_CHECK(stack.progress() == Progress(1,2));
        second.finish(); drain();
        BOOST_CHECK_EQUAL(stack.value(), 100);
    }
    drain();
    BOOST_CHECK_EQUAL(stack.value(), 100); // completed tasks retain weight
    stack.clear(); BOOST_CHECK_EQUAL(stack.value(), 0);
    BOOST_CHECK(stack.progress() == Progress(0));
}
BOOST_AUTO_TEST_CASE(cancellation_and_clear) {
    StackedProgressBar stack;
    auto first = stack.makeNotifier(); auto second = stack.makeNotifier();
    first.setStepCount(2); second.setStepCount(2);
    first.increment(); drain();
    stack.cancel();
    BOOST_CHECK_THROW(first.increment(), CancelException);
    BOOST_CHECK_THROW(second.increment(), CancelException);
    drain(); BOOST_CHECK_EQUAL(stack.value(), 25);
    stack.clear();
    BOOST_CHECK(first.stopToken().stop_requested());
    auto replacement = stack.makeNotifier(); replacement.setStepCount(1);
    replacement.finish(); drain(); BOOST_CHECK_EQUAL(stack.value(), 100);
}
BOOST_AUTO_TEST_CASE(widget_destruction_during_work) {
    auto bar = std::make_unique<ProgressBar>();
    auto handle = bar->makeNotifier(); handle.setStepCount(1000);
    std::atomic<bool> started = false;
    std::jthread worker([handle = std::move(handle), &started]() mutable {
        started = true;
        try { for (int i = 0; i < 1000; ++i) handle.increment(); } catch (const CancelException&) {}
    });
    while (!started) std::this_thread::yield();
    bar.reset(); worker.join(); drain();
    auto adapter = std::make_unique<QtProgressNotifier>();
    auto survivor = adapter->makeNotifier(); survivor.setStepCount(1);
    adapter.reset(); BOOST_CHECK_THROW(survivor.increment(), CancelException);
}
BOOST_AUTO_TEST_CASE(dialog_outcomes_and_buttons) {
    ProgressDialog success;
    { auto h = success.makeNotifier(); h.setStepCount(1); h.finish(); }
    drain(); BOOST_CHECK_EQUAL(success.result(), QDialog::Accepted);
    ProgressDialog canceled;
    { auto h = canceled.makeNotifier(); h.setStepCount(2);
      auto* buttons = canceled.findChild<QDialogButtonBox*>(); BOOST_REQUIRE(buttons);
      buttons->button(QDialogButtonBox::Cancel)->click();
      BOOST_CHECK_THROW(h.increment(), CancelException); }
    drain(); BOOST_CHECK_EQUAL(canceled.result(), QDialog::Rejected);
    ProgressDialog failed; failed.show();
    try { auto h = failed.makeNotifier(); h.setStepCount(1); throw 5; } catch (int) {}
    drain(); BOOST_CHECK_EQUAL(failed.result(), QDialog::Rejected); BOOST_CHECK(!failed.isVisible());
    ProgressDialog abandoned; abandoned.show();
    { auto h = abandoned.makeNotifier(); }
    drain(); BOOST_CHECK_EQUAL(abandoned.result(), QDialog::Rejected); BOOST_CHECK(!abandoned.isVisible());
    ProgressDialog closed;
    { auto h = closed.makeNotifier(); h.setStepCount(1); closed.show(); closed.close();
      BOOST_CHECK(h.stopToken().stop_requested()); }
    drain(); BOOST_CHECK_EQUAL(closed.result(), QDialog::Rejected);
    ProgressDialog lateCancel;
    { auto h = lateCancel.makeNotifier(); h.setStepCount(1); h.finish(); }
    lateCancel.reject(); drain(); BOOST_CHECK_EQUAL(lateCancel.result(), QDialog::Rejected);
}
BOOST_AUTO_TEST_CASE(nested_core_tasks) {
    ProgressBar bar;
    { auto root = bar.makeNotifier(); root.setStepCount(2);
      { auto child = root.subTaskNotifier(); child.setStepCount(2); child.increment(); drain();
        BOOST_CHECK_EQUAL(bar.value(), 25); }
      drain(); BOOST_CHECK_EQUAL(bar.value(), 50);
      root.finish(); }
    drain(); BOOST_CHECK_EQUAL(bar.value(), 100);
}

BOOST_AUTO_TEST_CASE(receiver_deleted_from_signal) {
    auto adapter = std::make_unique<QtProgressNotifier>();
    auto handle = adapter->makeNotifier(); handle.setStepCount(1);
    QObject::connect(adapter.get(), &QtProgressNotifier::progressChanged,
                     [&](Progress) { adapter.reset(); });
    handle.finish(); drain();
    BOOST_CHECK(!adapter);
    BOOST_CHECK(handle.stopToken().stop_requested());
    auto terminal = std::make_unique<QtProgressNotifier>();
    QObject::connect(terminal.get(), &QtProgressNotifier::stopped,
                     [&](Completion) { terminal.reset(); });
    { auto h = terminal->makeNotifier(); h.setStepCount(1); h.finish(); }
    drain(); BOOST_CHECK(!terminal);
}
BOOST_AUTO_TEST_CASE(clear_with_pending_events) {
    StackedProgressBar stack;
    auto old = stack.makeNotifier(); old.setStepCount(2); old.increment();
    stack.clear(); // old update is still queued
    auto fresh = stack.makeNotifier(); fresh.setStepCount(4); fresh.increment();
    drain(); BOOST_CHECK(stack.progress() == Progress(1,4));
    BOOST_CHECK_EQUAL(stack.value(), 25);
    BOOST_CHECK(old.stopToken().stop_requested());
}
