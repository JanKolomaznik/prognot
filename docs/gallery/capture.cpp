// Deterministic captures of real widgets, not hand-drawn progress bars.
#include <prognot/Qt/ProgressDialog.hpp>
#include <prognot/Qt/StackedProgressBar.hpp>
#include <prognot/Qt/PopupProgressBar.hpp>
#include <QMouseEvent>
#include <QApplication>
#include <QDir>
#include <QEvent>
#include <QImage>
#include <QPainter>
#include <QStyleFactory>
#include <algorithm>
#include <stdexcept>

using namespace prognot;
using namespace prognot::qt;
static void drain() {
    QCoreApplication::sendPostedEvents(nullptr, QEvent::MetaCall);
    QCoreApplication::processEvents();
}
static void frame(const QDir& directory, const QString& scene, int index,
                  const QString& title, const QString& caption, QWidget& widget,
                  QWidget* popup = nullptr) {
    QImage image(600, 280, QImage::Format_RGB32);
    image.fill(QColor("#f4f6f8"));
    QPainter painter(&image);
    painter.setPen(QColor("#182430"));
    QFont font("DejaVu Sans", 15, QFont::DemiBold);
    painter.setFont(font);
    painter.drawText(QRect(24, 14, 552, 30), Qt::AlignLeft | Qt::AlignVCenter, title);
    font.setPointSize(10); font.setWeight(QFont::Normal); painter.setFont(font);
    painter.setPen(QColor("#425466"));
    painter.drawText(QRect(24, 242, 552, 28), Qt::AlignLeft | Qt::AlignVCenter, caption);
    painter.drawPixmap(40, popup ? 172 : 85, widget.grab());
    if (popup) painter.drawPixmap(40, 55, popup->grab());
    painter.end();
    if (!image.save(directory.filePath(scene + QString("-%1.png").arg(index, 3, 10, QChar('0')))))
        throw std::runtime_error("Cannot save gallery frame");
}
int main(int argc, char** argv) {
    QApplication app(argc, argv);
    if (argc != 2) return 1;
    app.setStyle(QStyleFactory::create("Fusion"));
    QPalette palette;
    palette.setColor(QPalette::Window, QColor("#f4f6f8"));
    palette.setColor(QPalette::WindowText, QColor("#182430"));
    palette.setColor(QPalette::Base, Qt::white);
    palette.setColor(QPalette::Text, QColor("#182430"));
    palette.setColor(QPalette::Button, QColor("#e7edf3"));
    palette.setColor(QPalette::ButtonText, QColor("#182430"));
    palette.setColor(QPalette::Highlight, QColor("#2979b8"));
    palette.setColor(QPalette::HighlightedText, Qt::white);
    app.setPalette(palette);
    app.setFont(QFont("DejaVu Sans", 10));
    QDir directory(argv[1]);
    if (!directory.exists()) return 2;
    constexpr int frames = 64;
    {
        ProgressBar bar; bar.setFixedSize(520, 34); bar.show();
        auto task = bar.makeNotifier(); task.setStepCount(100);
        int previous = 0;
        for (int i = 0; i < frames; ++i) {
            const int next = std::clamp((i - 5) * 2, 0, 100);
            if (next > previous) task.increment(next - previous);
            previous = next; drain();
            frame(directory, "progress-bar", i, "ProgressBar", "One algorithm. UI-independent progress reporting.", bar);
        }
    }
    {
        ProgressDialog dialog; dialog.setText("Loading assets and building the index...");
        dialog.setFixedSize(520, 120); dialog.show();
        auto task = dialog.makeNotifier(); task.setStepCount(100);
        int previous = 0;
        for (int i = 0; i < frames; ++i) {
            const int next = std::clamp((i - 5) * 2, 0, 100);
            if (next > previous) task.increment(next - previous);
            previous = next;
            drain();
            frame(directory, "progress-dialog", i, "ProgressDialog", "Progress with cooperative cancellation.", dialog);
        }
    }
    {
        StackedProgressBar group; group.setFixedSize(520, 60); group.show();
        auto first = group.makeNotifier(1);
        auto second = group.makeNotifier(2);
        auto third = group.makeNotifier(3);
        first.setStepCount(100); second.setStepCount(100); third.setStepCount(100);
        // ProgressBarStack has no Q_OBJECT, so use RTTI rather than qobject_cast.
        ProgressBarStack* popup = nullptr;
        for (auto* child : group.children())
            if (auto* candidate = dynamic_cast<ProgressBarStack*>(child)) popup = candidate;
        if (!popup) return 3;
        int oldA = 0, oldB = 0, oldC = 0;
        for (int i = 0; i < frames; ++i) {
            int a = std::clamp((i-5)*4, 0, 100);
            int b = std::clamp((i-5)*3, 0, 100);
            int c = std::clamp((i-5)*2, 0, 100);
            if (a > oldA) first.increment(a-oldA);
            if (b > oldB) second.increment(b-oldB);
            if (c > oldC) third.increment(c-oldC);
            oldA=a; oldB=b; oldC=c;
            drain(); group.showSubtaskWindow(true); drain();
            frame(directory, "stacked-progress", i, "StackedProgressBar", "Three independent tasks. Weights 1 : 2 : 3.", group, popup);
        }
    }
    {
        PopupProgressBar group; group.setFixedSize(520, 60);
        group.setPopupDirection(PopupProgressBar::PopupDirection::Up);
        group.show();
        auto first = group.makeNotifier(1); auto second = group.makeNotifier(2);
        first.setStepCount(100); second.setStepCount(100);
        ProgressBarStack* popup = nullptr;
        for (auto* child : group.children())
            if (auto* candidate = dynamic_cast<ProgressBarStack*>(child)) popup = candidate;
        if (!popup) return 4;
        int oldA = 0, oldB = 0;
        for (int i = 0; i < frames; ++i) {
            const int a = std::clamp((i-5)*3, 0, 100);
            const int b = std::clamp((i-5)*2, 0, 100);
            if (a > oldA) first.increment(a-oldA);
            if (b > oldB) second.increment(b-oldB);
            oldA = a; oldB = b; drain();
            if (i == 12) {
                QMouseEvent click(QEvent::MouseButtonPress, QPointF(5, 5),
                                  QPointF(group.mapToGlobal(QPoint(5, 5))),
                                  Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
                QApplication::sendEvent(&group, &click); drain();
            }
            frame(directory, "popup-progress", i, "PopupProgressBar",
                  "Click to open details. Configurable direction and hover activation.",
                  group, group.isSubtaskWindowVisible() ? popup : nullptr);
        }
    }

}
