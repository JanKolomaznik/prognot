#include <QApplication>

#include <QWidget>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

#include <thread>
#include <chrono>

#include "prognot/prognot.hpp"
#include "prognot/Qt/StackedProgressBar.hpp"

void computation(int seconds, prognot::ProgressNotifier aNotifier)
{
	aNotifier.setStepCount(50);
	for (int i = 0; i < 50; ++i) {
		std::this_thread::sleep_for (std::chrono::milliseconds(seconds * 1000 / 50));
		aNotifier.increment();
	}
}


class MainWindow : public QWidget
{
public:
	explicit MainWindow(QWidget *parent = nullptr)
		: QWidget(parent)
	{
		auto *layout = new QVBoxLayout();
		mStackedProgressBar = new prognot::qt::StackedProgressBar();
		layout->addWidget(mStackedProgressBar);

		mStartButton = new QPushButton();
		mStartButton->setText("Start");
		layout->addWidget(mStartButton);

		mSpinBox = new QSpinBox();
		mSpinBox->setValue(8);
		layout->addWidget(mSpinBox);
		setLayout(layout);

		QObject::connect(
			mStartButton,
			&QPushButton::clicked,
			this,
			&MainWindow::run);
	}

	void run()
	{
		auto progressNotifier = mStackedProgressBar->createProgressNotifier();
		mThread = std::thread([this, progressNotifier]() {
			try {
				computation(this->mSpinBox->value(), progressNotifier);
			} catch (prognot::CancelException &) {

			}
		});
		mThread.detach();
	}

	prognot::qt::StackedProgressBar *mStackedProgressBar;
	QPushButton *mStartButton;
	QSpinBox *mSpinBox;
	std::thread mThread;
};

int
main(int argc, char *argv[]) {
	QApplication app(argc, argv);

	MainWindow win;
	win.show();
	return app.exec();
}
