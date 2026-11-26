#pragma once
/** @file PopupProgressBar.hpp @brief Configurable popup view of weighted operations. */
#include <prognot/Qt/StackedProgressBar.hpp>

namespace prognot::qt {
/**
 * @brief Weighted progress with a directional popup opened by click or hover.
 * GUI-thread API. Inherits task ownership, exact aggregation and cancellation
 * from StackedProgressBar. Escape and outside clicks dismiss the popup.
 */
class PopupProgressBar : public StackedProgressBar {
    Q_OBJECT
public:
    /** @brief Preferred side of the aggregate; placement stays within the screen. */
    enum class PopupDirection { Up, Down, Left, Right };
    Q_ENUM(PopupDirection)
    /** @brief Interaction that opens details. Hover keeps them open for inspection. */
    enum class PopupTrigger { Click, Hover };
    Q_ENUM(PopupTrigger)
    /** @brief Construct with downward placement and click activation. @param parent Owner. */
    explicit PopupProgressBar(QWidget* parent = nullptr);
    /** @brief Change preferred placement. @param direction Side of the aggregate. */
    void setPopupDirection(PopupDirection direction);
    /** @return Preferred placement. */
    PopupDirection popupDirection() const noexcept { return direction_; }
    /** @brief Change activation and hide existing details. @param trigger Interaction. */
    void setPopupTrigger(PopupTrigger trigger);
    /** @return Current activation mode. */
    PopupTrigger popupTrigger() const noexcept { return trigger_; }
    /** @return Whether details are currently visible. */
    bool isSubtaskWindowVisible() const;
public slots:
    /** @brief Show or hide details regardless of trigger. @param show Desired visibility. */
    void showSubtaskWindow(bool show = true) override;
protected:
    /** @brief Handle hover activation. @param event Pointer entry. */
    void enterEvent(QEnterEvent* event) override;
    /** @brief Keep an open popup aligned. @param event New widget dimensions. */
    void resizeEvent(QResizeEvent* event) override;
    /** @brief Keep an open popup aligned. @param event New widget position. */
    void moveEvent(QMoveEvent* event) override;
    /** @brief Hide details with the owner. @param event Visibility change. */
    void hideEvent(QHideEvent* event) override;
    /** @brief Observe clicks on the display. @param watched Event receiver. @param event Input event. @return Whether consumed. */
    bool eventFilter(QObject* watched, QEvent* event) override;
    /** @brief Activate from the widget margins. @param event Mouse press. */
    void mousePressEvent(QMouseEvent* event) override;
private:
    void positionPopup();
    PopupDirection direction_ = PopupDirection::Down;
    PopupTrigger trigger_ = PopupTrigger::Click;
};
} // namespace prognot::qt
