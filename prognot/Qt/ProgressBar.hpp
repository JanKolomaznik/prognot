#pragma once

#include <QProgressBar>
#include <prognot/Qt/ProgressNotifier.hpp>

namespace prognot {
namespace qt {

class ProgressBar : public QProgressBar
{
public:
	ProgressBar(QWidget *parent = nullptr)
		: QProgressBar(parent)
	{
		setMaximum(100);
		setMinimum(0);

		QObject::connect(
			&mProgressNotifier,
			&QtProgressNotifier::incremented,
			this,
			&QProgressBar::setValue,
			Qt::QueuedConnection);
	}

	QtProgressNotifier *
	progressNotifier()
	{
		return &mProgressNotifier;
	}
protected:
	qt::QtProgressNotifier mProgressNotifier;
};

} // namespace qt
} // namespace prognot
