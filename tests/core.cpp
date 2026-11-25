#define BOOST_TEST_MODULE prognot_core
#include <boost/test/included/unit_test.hpp>

#include <prognot/prognot.hpp>

#include <algorithm>
#include <limits>
#include <thread>
#include <vector>

using namespace prognot;
struct Sink : AProgressNotifierImpl {
    std::vector<Progress> values;
    std::vector<Completion> results;
    int starts = 0, finishes = 0;
    bool keepGoing = true, throwIncrement = false, throwFinal = false;
    bool onIncrement(Progress p) override {
        values.push_back(std::move(p));
        if (throwIncrement) throw 42;
        return keepGoing;
    }
    void onStarted() override { ++starts; }
    void onStopped(Completion result) override {
        results.push_back(result);
        if (throwFinal) throw 42;
    }
    void onFinished() override { ++finishes; if (throwFinal) throw 43; }
};
BOOST_AUTO_TEST_CASE(batch_and_scope) {
    auto s = std::make_shared<Sink>();
    { ProgressNotifier p(s); p.setStepCount(10); p.increment(5); BOOST_CHECK(p.progress() == Progress(1,2)); }
    BOOST_CHECK(s->values.back() == Progress(1));
    BOOST_CHECK(s->starts == 1 && s->finishes == 1 && s->results.back() == Completion::succeeded);
}
BOOST_AUTO_TEST_CASE(hierarchy) {
    auto s = std::make_shared<Sink>();
    {
        ProgressNotifier p(s); p.setStepCount(10);
        { auto child = p.subTaskNotifier(3); child.setStepCount(2); child.increment();
          BOOST_CHECK(p.progress() == Progress(3,20)); }
        BOOST_CHECK(p.progress() == Progress(3,10));
        p.finish(); BOOST_CHECK(p.progress() == Progress(1));
        BOOST_CHECK_THROW(([&]{p.increment();}()), std::logic_error);
    }
    BOOST_CHECK(s->results.back() == Completion::succeeded);
    auto s2 = std::make_shared<Sink>();
    { ProgressNotifier p(s2); p.setStepCount(1); auto unconfigured = p.subTaskNotifier(); }
    BOOST_CHECK(s2->results.back() == Completion::abandoned);
}
BOOST_AUTO_TEST_CASE(validation) {
    BOOST_CHECK_THROW(([]{ProgressNotifier p(static_cast<AProgressNotifierImpl*>(nullptr));}()), std::invalid_argument);
    ProgressNotifier p;
    BOOST_CHECK_THROW(([&]{p.increment();}()), std::logic_error);
    BOOST_CHECK_THROW(([&]{(void)p.subTaskNotifier();}()), std::logic_error);
    BOOST_CHECK_THROW(([&]{p.setStepCount(0);}()), std::invalid_argument);
    p.setStepCount(3);
    BOOST_CHECK_THROW(([&]{p.increment(-1);}()), std::invalid_argument);
    BOOST_CHECK_THROW(([&]{(void)p.subTaskNotifier(4);}()), std::out_of_range);
    BOOST_CHECK_THROW(([&]{p.increment(4);}()), std::out_of_range);
    BOOST_CHECK(p.progress() == Progress(0));
    p.increment();
    BOOST_CHECK_THROW(([&]{p.setStepCount(4);}()), std::logic_error);
}
BOOST_AUTO_TEST_CASE(moves_and_lifetime) {
    auto s = std::make_shared<Sink>();
    std::weak_ptr<Sink> weak = s;
    {
        ProgressNotifier p(s); p.setStepCount(2);
        auto child = p.subTaskNotifier(); child.setStepCount(1);
        ProgressNotifier moved(std::move(p));
        BOOST_CHECK_THROW(([&]{p.increment();}()), std::logic_error);
        auto* same = &moved;
        moved = std::move(*same);
        moved.finish(); BOOST_CHECK(s->finishes == 0);
        s.reset(); BOOST_CHECK(!weak.expired());
        child.finish();
    }
    BOOST_CHECK(weak.expired());
    auto oldSink = std::make_shared<Sink>();
    { ProgressNotifier a(oldSink); a.setStepCount(1); ProgressNotifier b; a = std::move(b); }
    BOOST_CHECK(oldSink->results.back() == Completion::succeeded);
}
BOOST_AUTO_TEST_CASE(cancellation_and_failures) {
    auto s = std::make_shared<Sink>();
    { ProgressNotifier p(s); p.setStepCount(2); s->keepGoing = false;
      BOOST_CHECK_THROW(([&]{p.increment();}()), CancelException); BOOST_CHECK(p.stopToken().stop_requested()); }
    BOOST_CHECK(s->values.size() == 1 && s->results.back() == Completion::canceled);
    auto t = std::make_shared<Sink>();
    { ProgressNotifier p(t); p.setStepCount(1); auto source = p.stopSource();
      std::jthread worker([source]() mutable { source.request_stop(); }); worker.join();
      BOOST_CHECK_THROW(([&]{p.checkCancellation();}()), CancelException);
      BOOST_CHECK_THROW(([&]{p.finish();}()), CancelException); }
    BOOST_CHECK(t->values.empty() && t->results.back() == Completion::canceled);
    auto f = std::make_shared<Sink>();
    try { ProgressNotifier p(f); p.setStepCount(2); p.increment(); throw 12; } catch (int) {}
    BOOST_CHECK(f->values.size() == 1 && f->results.back() == Completion::failed);
    auto e = std::make_shared<Sink>(); e->throwIncrement = true; e->throwFinal = true;
    { ProgressNotifier p(e); p.setStepCount(2); BOOST_CHECK_THROW(([&]{p.increment();}()), int);
      BOOST_CHECK_THROW(([&]{p.increment();}()), std::logic_error); }
    BOOST_CHECK(e->values.size() == 1 && e->finishes == 1 && e->results.back() == Completion::failed);
}
BOOST_AUTO_TEST_CASE(iterators) {
    using It = InputIteratorProgressNotifierAdaptor<std::vector<int>::iterator>;
    static_assert(std::input_iterator<It>);
    static_assert(!std::forward_iterator<It>);
    {
        ProgressNotifier p; p.setStepCount(1);
        std::vector<int> input{1, 2, 3}, output;
        It first(input.begin(), p.subTaskNotifier(), input.size());
        It last(input.end(), ProgressNotifier{}, 0);
        BOOST_CHECK(*first++ == 1);
        BOOST_CHECK(first.base() == input.begin() + 1);
        std::copy(first, last, std::back_inserter(output));
        BOOST_CHECK((output == std::vector<int>{2, 3}));
        BOOST_CHECK(p.progress() == Progress(1));
    }
    for (int size : {0, 1, 3, 99, 100, 101, 1001}) {
        auto s = std::make_shared<Sink>();
        { ProgressNotifier p(s); p.setStepCount(1);
          std::vector<int> v(size);
          InputIteratorProgressNotifierAdaptor it(v.begin(), p.subTaskNotifier(), size);
          auto copy = it;
          for (int i = 0; i < size; ++i) { ++it; BOOST_CHECK(p.progress() == Progress(i+1,size)); }
          BOOST_CHECK(p.progress() == Progress(1));
          BOOST_CHECK_THROW(([&]{++copy;}()), std::out_of_range); }
        BOOST_CHECK(s->results.back() == Completion::succeeded);
    }
    ProgressNotifier p;
    std::vector<int> v;
    BOOST_CHECK_THROW(([&]{InputIteratorProgressNotifierAdaptor it(v.begin(),std::move(p),-1);}()), std::invalid_argument);
}
BOOST_AUTO_TEST_CASE(concurrent_children) {
    auto s = std::make_shared<Sink>();
    { ProgressNotifier p(s); p.setStepCount(8);
      std::vector<std::jthread> threads;
      for (int i = 0; i < 8; ++i) {
          auto child = p.subTaskNotifier(); child.setStepCount(100);
          threads.emplace_back([child = std::move(child)]() mutable {
              for (int j = 0; j < 100; ++j) child.increment();
          });
      }
      threads.clear(); BOOST_CHECK(p.progress() == Progress(1)); }
    BOOST_CHECK(s->values.size() == 800 && std::is_sorted(s->values.begin(),s->values.end()));
}
BOOST_AUTO_TEST_CASE(callback_contract) {
    struct Reentrant : Sink {
        ProgressNotifier* handle = nullptr;
        bool onIncrement(Progress value) override {
            BOOST_CHECK(handle->progress() == value);
            BOOST_CHECK_THROW(([&]{ handle->increment(); }()), std::logic_error);
            BOOST_CHECK_THROW(([&]{ handle->setStepCount(1); }()), std::logic_error);
            BOOST_CHECK_THROW(([&]{ (void)handle->subTaskNotifier(); }()), std::logic_error);
            return Sink::onIncrement(value);
        }
    };
    auto s = std::make_shared<Reentrant>();
    { ProgressNotifier p(s); s->handle = &p; p.setStepCount(1); p.finish(); }
    BOOST_CHECK(s->results.back() == Completion::succeeded);
    struct BadStart : Sink { void onStarted() override { throw 7; } };
    auto bad = std::make_shared<BadStart>();
    BOOST_CHECK_THROW(([&]{ ProgressNotifier p(bad); }()), int);
    BOOST_CHECK(bad->finishes == 0 && bad->results.empty());
    auto canceled = std::make_shared<Sink>();
    { ProgressNotifier p(canceled); p.setStepCount(1);
      auto child = p.subTaskNotifier(); child.setStepCount(1);
      p.requestCancel();
      BOOST_CHECK_THROW(([&]{ child.increment(); }()), CancelException); }
    BOOST_CHECK(canceled->values.empty() && canceled->results.back() == Completion::canceled);
}
BOOST_AUTO_TEST_CASE(early_iterator_exit) {
    auto s = std::make_shared<Sink>();
    { ProgressNotifier p(s); p.setStepCount(1); std::vector<int> v(3);
      { InputIteratorProgressNotifierAdaptor it(v.begin(),p.subTaskNotifier(),3); ++it; }
      BOOST_CHECK(p.progress() == Progress(1)); }
    BOOST_CHECK(s->results.back() == Completion::succeeded);
}
BOOST_AUTO_TEST_CASE(deep_arithmetic) {
    ProgressNotifier p; p.setStepCount(std::numeric_limits<std::int64_t>::max());
    auto a = p.subTaskNotifier(); a.setStepCount(std::numeric_limits<std::int64_t>::max());
    auto b = a.subTaskNotifier(); b.setStepCount(std::numeric_limits<std::int64_t>::max());
    b.increment(); BOOST_CHECK(p.progress() > Progress(0) && p.progress() < Progress(1));
}

namespace {
struct BoolCallback { bool operator()(const Progress&) { return true; } };
struct VoidCallback { void operator()(Progress) {} };
struct IntCallback { int operator()(Progress) { return 1; } };
struct WrongArgument { bool operator()(const char*) { return true; } };
struct RvalueOnlyCallback { bool operator()(Progress) && { return true; } };
struct MoveOnlyCallback {
    std::unique_ptr<int> calls = std::make_unique<int>(0);
    bool operator()(Progress) { ++*calls; return true; }
};
template<class T>
concept CanMakeNotifier = requires(T&& callback) {
    { makeProgressNotifier(std::forward<T>(callback)) } -> std::same_as<ProgressNotifier>;
};
template<class T>
concept CanAdaptIterator = requires { typename InputIteratorProgressNotifierAdaptor<T>; };
static_assert(ProgressCallback<BoolCallback>);
static_assert(ProgressCallback<bool(*)(Progress)>);
static_assert(!ProgressCallback<VoidCallback>);
static_assert(!ProgressCallback<IntCallback>);
static_assert(!ProgressCallback<WrongArgument>);
static_assert(!ProgressCallback<RvalueOnlyCallback>);
static_assert(!ProgressCallback<int>);
static_assert(CanMakeNotifier<MoveOnlyCallback>);
static_assert(!CanMakeNotifier<MoveOnlyCallback&>);
static_assert(!CanMakeNotifier<VoidCallback>);
static_assert(CanAdaptIterator<int*>);
static_assert(!CanAdaptIterator<int>);
static_assert(!CanAdaptIterator<std::back_insert_iterator<std::vector<int>>>);
}

BOOST_AUTO_TEST_CASE(constrained_callbacks) {
    std::vector<Progress> values;
    {
        auto notifier = makeProgressNotifier([&](const Progress& p) {
            values.push_back(p);
            return true;
        });
        notifier.setStepCount(2);
        notifier.increment();
        notifier.finish();
    }
    BOOST_REQUIRE_EQUAL(values.size(), 2U);
    BOOST_CHECK(values[0] == Progress(1, 2));
    BOOST_CHECK(values[1] == Progress(1));

    auto calls = std::make_unique<int>(0);
    auto* observed = calls.get();
    auto notifier = makeProgressNotifier([calls = std::move(calls)](Progress) mutable {
        return ++*calls < 2;
    });
    notifier.setStepCount(3);
    notifier.increment();
    BOOST_CHECK_THROW(notifier.increment(), CancelException);
    BOOST_CHECK_EQUAL(*observed, 2);
    BOOST_CHECK(notifier.stopToken().stop_requested());

    auto function = +[](Progress) { return true; };
    auto fromFunction = makeProgressNotifier(function);
    fromFunction.setStepCount(1);
    BOOST_CHECK_NO_THROW(fromFunction.finish());
}
