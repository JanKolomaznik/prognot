#include <prognot/Qt/StackedProgressBar.hpp>
#include <QApplication>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>
#include <chrono>
#include <thread>
#include <vector>
int main(int argc, char** argv) {
    QApplication app(argc, argv);
    QWidget window;
    auto* layout = new QVBoxLayout(&window);
    auto* stack = new prognot::qt::StackedProgressBar(&window);
    auto* seconds = new QSpinBox(&window); seconds->setRange(1, 60); seconds->setValue(5);
    auto* start = new QPushButton("Add task (weight = seconds)", &window);
    auto* cancel = new QPushButton("Cancel all", &window);
    layout->addWidget(stack); layout->addWidget(seconds); layout->addWidget(start); layout->addWidget(cancel);
    std::vector<std::jthread> workers;
    QObject::connect(cancel, &QPushButton::clicked, stack, &prognot::qt::StackedProgressBar::cancel);
    QObject::connect(start, &QPushButton::clicked, &window, [&] {
        const auto duration = seconds->value(); // read widgets only on GUI thread
        auto handle = stack->makeNotifier(duration);
        workers.emplace_back([duration, handle = std::move(handle)](std::stop_token stop) mutable {
            try {
                handle.setStepCount(100);
                for (int i = 0; i < 100; ++i) {
                    if (stop.stop_requested()) handle.cancel();
                    std::this_thread::sleep_for(std::chrono::milliseconds(duration * 10));
                    handle.increment();
                }
            } catch (const prognot::CancelException&) {}
        });
    });
    window.show();
    const auto result = app.exec();
    for (auto& worker : workers) worker.request_stop();
    return result;
}
