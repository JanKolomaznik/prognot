#include <prognot/prognot.hpp>

#include <algorithm>
#include <functional>
#include <iostream>
#include <limits>
#include <thread>
#include <string_view>
#include <vector>

using namespace prognot;
void check(bool ok) { if (!ok) throw std::runtime_error("Check failed"); }
template<class E, class F> void throws(F f) {
    try { f(); } catch (const E&) { return; }
    throw std::runtime_error("Expected exception");
}
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
void batchAndScope() {
    auto s = std::make_shared<Sink>();
    { ProgressNotifier p(s); p.setStepCount(10); p.increment(5); check(p.progress() == Progress(1,2)); }
    check(s->values.back() == Progress(1));
    check(s->starts == 1 && s->finishes == 1 && s->results.back() == Completion::succeeded);
}
void hierarchy() {
    auto s = std::make_shared<Sink>();
    {
        ProgressNotifier p(s); p.setStepCount(10);
        { auto child = p.subTaskNotifier(3); child.setStepCount(2); child.increment();
          check(p.progress() == Progress(3,20)); }
        check(p.progress() == Progress(3,10));
        p.finish(); check(p.progress() == Progress(1));
        throws<std::logic_error>([&]{p.increment();});
    }
    check(s->results.back() == Completion::succeeded);
    auto s2 = std::make_shared<Sink>();
    { ProgressNotifier p(s2); p.setStepCount(1); auto unconfigured = p.subTaskNotifier(); }
    check(s2->results.back() == Completion::abandoned);
}
void validation() {
    throws<std::invalid_argument>([]{ProgressNotifier p(static_cast<AProgressNotifierImpl*>(nullptr));});
    ProgressNotifier p;
    throws<std::logic_error>([&]{p.increment();});
    throws<std::logic_error>([&]{(void)p.subTaskNotifier();});
    throws<std::invalid_argument>([&]{p.setStepCount(0);});
    p.setStepCount(3);
    throws<std::invalid_argument>([&]{p.increment(-1);});
    throws<std::out_of_range>([&]{(void)p.subTaskNotifier(4);});
    throws<std::out_of_range>([&]{p.increment(4);});
    check(p.progress() == Progress(0));
    p.increment();
    throws<std::logic_error>([&]{p.setStepCount(4);});
}
void movesAndLifetime() {
    auto s = std::make_shared<Sink>();
    std::weak_ptr<Sink> weak = s;
    {
        ProgressNotifier p(s); p.setStepCount(2);
        auto child = p.subTaskNotifier(); child.setStepCount(1);
        ProgressNotifier moved(std::move(p));
        throws<std::logic_error>([&]{p.increment();});
        auto* same = &moved;
        moved = std::move(*same);
        moved.finish(); check(s->finishes == 0);
        s.reset(); check(!weak.expired());
        child.finish();
    }
    check(weak.expired());
    auto oldSink = std::make_shared<Sink>();
    { ProgressNotifier a(oldSink); a.setStepCount(1); ProgressNotifier b; a = std::move(b); }
    check(oldSink->results.back() == Completion::succeeded);
}
void cancellationAndFailures() {
    auto s = std::make_shared<Sink>();
    { ProgressNotifier p(s); p.setStepCount(2); s->keepGoing = false;
      throws<CancelException>([&]{p.increment();}); check(p.stopToken().stop_requested()); }
    check(s->values.size() == 1 && s->results.back() == Completion::canceled);
    auto t = std::make_shared<Sink>();
    { ProgressNotifier p(t); p.setStepCount(1); auto source = p.stopSource();
      std::jthread worker([source]() mutable { source.request_stop(); }); worker.join();
      throws<CancelException>([&]{p.checkCancellation();});
      throws<CancelException>([&]{p.finish();}); }
    check(t->values.empty() && t->results.back() == Completion::canceled);
    auto f = std::make_shared<Sink>();
    try { ProgressNotifier p(f); p.setStepCount(2); p.increment(); throw 12; } catch (int) {}
    check(f->values.size() == 1 && f->results.back() == Completion::failed);
    auto e = std::make_shared<Sink>(); e->throwIncrement = true; e->throwFinal = true;
    { ProgressNotifier p(e); p.setStepCount(2); throws<int>([&]{p.increment();});
      throws<std::logic_error>([&]{p.increment();}); }
    check(e->values.size() == 1 && e->finishes == 1 && e->results.back() == Completion::failed);
}
void iterators() {
    for (int size : {0, 1, 3, 99, 100, 101, 1001}) {
        auto s = std::make_shared<Sink>();
        { ProgressNotifier p(s); p.setStepCount(1);
          std::vector<int> v(size);
          InputIteratorProgressNotifierAdaptor it(v.begin(), p.subTaskNotifier(), size);
          auto copy = it;
          for (int i = 0; i < size; ++i) { ++it; check(p.progress() == Progress(i+1,size)); }
          check(p.progress() == Progress(1));
          throws<std::out_of_range>([&]{++copy;}); }
        check(s->results.back() == Completion::succeeded);
    }
    ProgressNotifier p;
    std::vector<int> v;
    throws<std::invalid_argument>([&]{InputIteratorProgressNotifierAdaptor it(v.begin(),std::move(p),-1);});
}
void concurrentChildren() {
    auto s = std::make_shared<Sink>();
    { ProgressNotifier p(s); p.setStepCount(8);
      std::vector<std::jthread> threads;
      for (int i = 0; i < 8; ++i) {
          auto child = p.subTaskNotifier(); child.setStepCount(100);
          threads.emplace_back([child = std::move(child)]() mutable {
              for (int j = 0; j < 100; ++j) child.increment();
          });
      }
      threads.clear(); check(p.progress() == Progress(1)); }
    check(s->values.size() == 800 && std::is_sorted(s->values.begin(),s->values.end()));
}
void callbackContract() {
    struct Reentrant : Sink {
        ProgressNotifier* handle = nullptr;
        bool onIncrement(Progress value) override {
            check(handle->progress() == value);
            throws<std::logic_error>([&]{ handle->increment(); });
            throws<std::logic_error>([&]{ handle->setStepCount(1); });
            throws<std::logic_error>([&]{ (void)handle->subTaskNotifier(); });
            return Sink::onIncrement(value);
        }
    };
    auto s = std::make_shared<Reentrant>();
    { ProgressNotifier p(s); s->handle = &p; p.setStepCount(1); p.finish(); }
    check(s->results.back() == Completion::succeeded);
    struct BadStart : Sink { void onStarted() override { throw 7; } };
    auto bad = std::make_shared<BadStart>();
    throws<int>([&]{ ProgressNotifier p(bad); });
    check(bad->finishes == 0 && bad->results.empty());
    auto canceled = std::make_shared<Sink>();
    { ProgressNotifier p(canceled); p.setStepCount(1);
      auto child = p.subTaskNotifier(); child.setStepCount(1);
      p.requestCancel();
      throws<CancelException>([&]{ child.increment(); }); }
    check(canceled->values.empty() && canceled->results.back() == Completion::canceled);
}
void earlyIteratorExit() {
    auto s = std::make_shared<Sink>();
    { ProgressNotifier p(s); p.setStepCount(1); std::vector<int> v(3);
      { InputIteratorProgressNotifierAdaptor it(v.begin(),p.subTaskNotifier(),3); ++it; }
      check(p.progress() == Progress(1)); }
    check(s->results.back() == Completion::succeeded);
}
void deepArithmetic() {
    ProgressNotifier p; p.setStepCount(std::numeric_limits<std::int64_t>::max());
    auto a = p.subTaskNotifier(); a.setStepCount(std::numeric_limits<std::int64_t>::max());
    auto b = a.subTaskNotifier(); b.setStepCount(std::numeric_limits<std::int64_t>::max());
    b.increment(); check(p.progress() > Progress(0) && p.progress() < Progress(1));
}
int main(int argc, char** argv) {
    const std::pair<const char*, void(*)()> tests[] = {
        {"batch and scope", batchAndScope}, {"hierarchy", hierarchy}, {"validation", validation},
        {"moves and lifetime", movesAndLifetime}, {"cancellation and failures", cancellationAndFailures},
        {"iterators", iterators}, {"concurrent children", concurrentChildren}, {"deep arithmetic", deepArithmetic}, {"callback contract", callbackContract},
        {"early iterator exit", earlyIteratorExit}
    };
    for (auto [name, run] : tests) {
        if (argc > 1 && std::string_view(argv[1]) != name) continue;
        try { run(); std::cout << "PASS " << name << '\n'; }
        catch (...) { std::cerr << "FAIL " << name << '\n'; return 1; }
    }
}
