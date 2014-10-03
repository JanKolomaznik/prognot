#include <prognot/Qt/StackedProgressBar.hpp>

#include <QResizeEvent>

namespace prognot {
namespace qt {

ProgressBarStack::ProgressBarStack(QWidget *parent)
	: QWidget(parent)
{
	setWindowFlags(Qt::Popup | Qt::FramelessWindowHint);
	//setWindowFlags(Qt::FramelessWindowHint);

	mLayout = new QVBoxLayout();
	setLayout(mLayout);
}

QtProgressNotifier *
ProgressBarStack::createProgressNotifier() {
	auto progressBar = new ProgressBar();
	mLayout->addWidget(progressBar);

	QObject::connect(progressBar, &ProgressBar::valueChanged, static_cast<StackedProgressBar *>(parent()), &StackedProgressBar::updateProgress );
	//QObject::connect(progressBar, &ProgressBar::finished, [this, progressBar]
	return progressBar->progressNotifier();
}

//---------------------------------------------------------

StackedProgressBar::StackedProgressBar(QWidget *parent)
	: QWidget(parent)
{
	auto *layout = new QVBoxLayout();

	mProgressBar = new QProgressBar();
	mProgressBar->setMinimum(0);
	mProgressBar->setMaximum(1000);

	layout->addWidget(mProgressBar);

	setLayout(layout);

	mProgressBarStack = new ProgressBarStack(this);
	//mProgressBarStack->resize(200, 200);
	mProgressBarStack->hide();

	setMouseTracking(true);
}

QtProgressNotifier *
StackedProgressBar::createProgressNotifier()
{
	return mProgressBarStack->createProgressNotifier();
	/*mProgressNotifiers.emplace_back();
	return &mProgressNotifiers.back();*/
}

void
StackedProgressBar::updateProgress()
{}

void
StackedProgressBar::showSubtaskWindow(bool aShow)
{
	if (aShow) {
		auto corner = mProgressBar->pos();
		corner.setY(corner.y() - mProgressBarStack->height());
		mProgressBarStack->move(mapToGlobal(corner));
		mProgressBarStack->show();
	} else {
		mProgressBarStack->hide();
	}
}

void
StackedProgressBar::enterEvent(QEvent * event)
{
	showSubtaskWindow();
	QWidget::enterEvent(event);
}

void
StackedProgressBar::resizeEvent(QResizeEvent *event)
{
	mProgressBarStack->setMinimumWidth(event->size().width());
	QWidget::resizeEvent(event);
}

} // namespace qt
} // namespace prognot
