#pragma once

#include <QProgressBar>
#include <QVBoxLayout>
#include <prognot/Qt/ProgressBar.hpp>

namespace prognot {
namespace qt {

class ProgressBarStack : public QWidget
{
	Q_OBJECT;
public:
	ProgressBarStack(QWidget *parent = nullptr);
	ProgressNotifier *
	createProgressNotifier();
protected:
	QVBoxLayout *mLayout;
};

class StackedProgressBar : public QWidget
{
	Q_OBJECT;
public:
	StackedProgressBar(QWidget *parent = nullptr);

	ProgressNotifier *
	createProgressNotifier();
public slots:
	void
	updateProgress();

	void
	showSubtaskWindow(bool aShow = true);
protected:
	void
	enterEvent(QEvent *event) override;

	/*void leaveEvent(QEvent * event) override {
		mProgressBarStack->hide();

		QWidget::leaveEvent(event);
	}*/

	void
	resizeEvent(QResizeEvent *event) override;

	struct SubTask
	{
		SubTask(ProgressBarStack *parent)
		{
			mProgressBar = new ProgressBar(parent);
		}

		ProgressBar *mProgressBar;
	};
	std::vector<ProgressNotifier> mProgressNotifiers;
	QProgressBar *mProgressBar;
	ProgressBarStack *mProgressBarStack;
};

} // namespace qt
} // namespace prognot
