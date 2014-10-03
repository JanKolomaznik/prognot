#include <QApplication>

#include <QWidget>
#include <QPushButton>
#include <QVBoxLayout>

#include <thread>
#include <chrono>

#include "prognot/prognot.hpp"
#include "prognot/Qt/ProgressBar.hpp"

void computation(prognot::ProgressNotifier aNotifier)
{
	aNotifier.setStepCount(50);
	for (int i = 0; i < 50; ++i) {
		std::this_thread::sleep_for (std::chrono::milliseconds(100));
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
		mProgressBar = new prognot::qt::ProgressBar();
		layout->addWidget(mProgressBar);

		mStartButton = new QPushButton();
		mStartButton->setText("Start");
		layout->addWidget(mStartButton);

		mCancelButton = new QPushButton();
		mCancelButton->setText("Cancel");
		layout->addWidget(mCancelButton);

		setLayout(layout);

		QObject::connect(
			mStartButton,
			&QPushButton::clicked,
			this,
			&MainWindow::run);

		QObject::connect(
			mCancelButton,
			&QPushButton::clicked,
			mProgressBar->progressNotifier(),
			&prognot::qt::ProgressNotifier::cancel);
	}

	void run()
	{
		mThread = std::thread([this]() {
			try {
				computation(mProgressBar->progressNotifier());
			} catch (prognot::CancelException &) {

			}
		});
		mThread.detach();
	}

	prognot::qt::ProgressBar *mProgressBar;
	QPushButton *mStartButton;
	QPushButton *mCancelButton;
	std::thread mThread;
};

int
main(int argc, char *argv[]) {
	QApplication app(argc, argv);

	MainWindow win;
	win.show();
	return app.exec();
}
