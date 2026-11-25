#include <prognot/Qt/ProgressBar.hpp>
#include <QApplication>
#include <QPushButton>
#include <QVBoxLayout>
#include <chrono>
#include <thread>
int main(int argc, char** argv) {
    QApplication app(argc, argv);
    QWidget window;
    auto* layout = new QVBoxLayout(&window);
    auto* bar = new prognot::qt::ProgressBar(&window);
    auto* start = new QPushButton("Start", &window);
    auto* cancel = new QPushButton("Cancel", &window);
    layout->addWidget(bar); layout->addWidget(start); layout->addWidget(cancel);
    std::jthread worker;
    QObject::connect(cancel, &QPushButton::clicked, bar->progressNotifier(), &prognot::qt::QtProgressNotifier::cancel);
    QObject::connect(start, &QPushButton::clicked, &window, [&] {
        start->setEnabled(false);
        worker = std::jthread([](std::stop_token stop, prognot::ProgressNotifier notifier) {
            try {
                notifier.setStepCount(50);
                for (int i = 0; i < 50; ++i) {
                    if (stop.stop_requested()) notifier.cancel();
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    notifier.increment();
                }
            } catch (const prognot::CancelException&) {}
        }, bar->makeNotifier());
    });
    window.show();
    return app.exec(); // worker requests stop and joins before window destruction
}
