#pragma once

#include <QObject>
#include <prognot/prognot.hpp>

namespace prognot {
namespace qt {

class ProgressNotifier
	: public QObject
	, public AProgressNotifierImpl
{
	Q_OBJECT;
public:
	explicit ProgressNotifier(QObject *parent = nullptr)
		: QObject(parent)
		, mShouldContinue(true)
	{}

	bool
	onIncrement(Progress aProgress) override
	{
		emit incremented(ceil(boost::rational_cast<float>(aProgress) * 100));
		return mShouldContinue;
	}

	void
	onStarted() override
	{
		mShouldContinue = true;
		emit started;
	}

	void
	onFinished() override
	{
		emit finished();
	}

signals:
	void
	incremented(int aPercentage);

	void
	started();

	void
	finished();

public slots:
	void
	cancel()
	{
		mShouldContinue = false;
	}

private:
	std::atomic<bool> mShouldContinue;
};

} // namespace qt
} // namespace prognot
