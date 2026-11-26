#include <prognot/Qt/PopupProgressBar.hpp>
#include <QEnterEvent>
#include <QHideEvent>
#include <QMouseEvent>
#include <QMoveEvent>
#include <QScreen>
#include <QResizeEvent>
#include <algorithm>

namespace prognot::qt {
PopupProgressBar::PopupProgressBar(QWidget* parent) : StackedProgressBar(parent) {
    bar_->installEventFilter(this);
    // An outside click on the aggregate dismisses details without reopening them.
    stack_->setAttribute(Qt::WA_NoMouseReplay);
}
void PopupProgressBar::setPopupDirection(PopupDirection direction) {
    direction_ = direction;
    if (stack_->isVisible()) positionPopup();
}
void PopupProgressBar::setPopupTrigger(PopupTrigger trigger) {
    trigger_ = trigger;
    showSubtaskWindow(false);
}
bool PopupProgressBar::isSubtaskWindowVisible() const { return stack_->isVisible(); }
void PopupProgressBar::positionPopup() {
    stack_->setMinimumWidth(bar_->width());
    stack_->adjustSize();
    const QRect anchor(bar_->mapToGlobal(QPoint(0, 0)), bar_->size());
    QPoint point;
    switch (direction_) {
    case PopupDirection::Up: point = anchor.topLeft() - QPoint(0, stack_->height()); break;
    case PopupDirection::Down: point = anchor.bottomLeft() + QPoint(0, 1); break;
    case PopupDirection::Left: point = anchor.topLeft() - QPoint(stack_->width(), 0); break;
    case PopupDirection::Right: point = anchor.topRight() + QPoint(1, 0); break;
    }
    if (auto* screen = bar_->screen()) {
        const auto area = screen->availableGeometry();
        point.setX(std::clamp(point.x(), area.left(), std::max(area.left(), area.right() - stack_->width() + 1)));
        point.setY(std::clamp(point.y(), area.top(), std::max(area.top(), area.bottom() - stack_->height() + 1)));
    }
    stack_->move(point);
}
void PopupProgressBar::showSubtaskWindow(bool show) {
    if (show) positionPopup();
    stack_->setVisible(show);
}
void PopupProgressBar::enterEvent(QEnterEvent* event) {
    if (trigger_ == PopupTrigger::Hover) showSubtaskWindow();
    QWidget::enterEvent(event);
}
void PopupProgressBar::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    if (stack_->isVisible()) positionPopup();
}
void PopupProgressBar::moveEvent(QMoveEvent* event) {
    QWidget::moveEvent(event);
    if (stack_->isVisible()) positionPopup();
}
void PopupProgressBar::hideEvent(QHideEvent* event) {
    showSubtaskWindow(false);
    QWidget::hideEvent(event);
}
bool PopupProgressBar::eventFilter(QObject* watched, QEvent* event) {
    if (watched == bar_ && event->type() == QEvent::MouseButtonPress &&
        trigger_ == PopupTrigger::Click && static_cast<QMouseEvent*>(event)->button() == Qt::LeftButton) {
        showSubtaskWindow(!stack_->isVisible());
        return true;
    }
    return StackedProgressBar::eventFilter(watched, event);
}
void PopupProgressBar::mousePressEvent(QMouseEvent* event) {
    if (trigger_ == PopupTrigger::Click && event->button() == Qt::LeftButton) {
        showSubtaskWindow(!stack_->isVisible());
        event->accept();
    } else QWidget::mousePressEvent(event);
}
} // namespace prognot::qt
