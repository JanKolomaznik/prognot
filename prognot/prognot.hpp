#pragma once

#include <atomic>
#include <cassert>
#include <memory>
#include <mutex>
#include <type_traits>
#include <iostream>

#include <boost/rational.hpp>
#include <boost/exception/all.hpp>
#include <boost/iterator/iterator_adaptor.hpp>

#define PROGNOT_ASSERT(x) assert(x)
#define PROGNOT_COUT std::cout
namespace prognot {

struct CancelException : std::exception, boost::exception {};

typedef boost::rational<int> Progress;

class AProgressNotifierImpl
{
public:

	virtual bool
	onIncrement(Progress aProgress) = 0;

	virtual void
	onStarted() {}

	virtual void
	onFinished() {}
protected:
};

class DummyProgressNotifier
	: public AProgressNotifierImpl
{
public:
	bool onIncrement(Progress /*progress*/) override
	{ return true; };
};

class ProgressNotifier
{
protected:
	struct NotifierInfo {
		NotifierInfo(AProgressNotifierImpl *aNotifierImplementation)
			: progress(0, 1)
			, canceled(false)
			, notifierImplementation(aNotifierImplementation)
		{
			PROGNOT_ASSERT(notifierImplementation);
			notifierImplementation->onStarted();
		}

		NotifierInfo(DummyProgressNotifier /*dummy_ProgressNotifier*/)
			: progress(0, 1)
			, canceled(false)
			, notifierImplementation(&dummy_notifier_)
		{}


		~NotifierInfo()
		{
			if (notifierImplementation) {
				notifierImplementation->onFinished();
			}
		}

		Progress progress;
		std::atomic<bool> canceled;
		AProgressNotifierImpl *notifierImplementation;
		std::mutex mtx;

		DummyProgressNotifier dummy_notifier_;  // Helper for situations when caller wants to ignore notifications - TODO(johny) find more efficient way
	};

public:
	ProgressNotifier(ProgressNotifier &&aNotifier)
		: mInfo(std::move(aNotifier.mInfo))
		, mProgressRange(std::move(aNotifier.mProgressRange))
		, mSingleStep(std::move(aNotifier.mSingleStep))
		, mStepCount(aNotifier.mStepCount)
		, mAssignedStepCount(aNotifier.mAssignedStepCount)
		, mCurrentLevel(aNotifier.mCurrentLevel)
	{}

	/*template<typename TImplementation>
	ProgressNotifier(TImplementation &&aImplementation)
		: mInfo(std::make_shared<NotifierInfo>())
		, mProgressRange(1, 1)
		, mStepCount(-1)
		, mCurrentLevel(0)
	{}*/

	ProgressNotifier(DummyProgressNotifier aDummyProgressNotifier)
		: mInfo(std::make_shared<NotifierInfo>(std::move(aDummyProgressNotifier)))
		, mProgressRange(1, 1)
		, mStepCount(-1)
		, mCurrentLevel(0)
	{}

	ProgressNotifier(AProgressNotifierImpl *aImplementation)
		: mInfo(std::make_shared<NotifierInfo>(aImplementation))
		, mProgressRange(1, 1)
		, mStepCount(-1)
		, mCurrentLevel(0)
	{}

	~ProgressNotifier() {
		PROGNOT_ASSERT(mAssignedStepCount <= mStepCount);
		if (!std::uncaught_exception() && mAssignedStepCount < mStepCount) {
			try {
				increment(mStepCount - mAssignedStepCount);
			} catch (CancelException &) {
				// Prevent terminate in case of cancel
			} catch (std::exception &e) {
				// Prevent terminate in case of an exception
				PROGNOT_COUT << "Caught exception in progress notifier destructor:\n" << boost::diagnostic_information(e) << std::endl;
			}
		}
	}

	ProgressNotifier &operator=(ProgressNotifier &&aNotifier)
	{
		mInfo = std::move(aNotifier.mInfo);
		mProgressRange = std::move(aNotifier.mProgressRange);
		mSingleStep = std::move(aNotifier.mSingleStep);
		mStepCount = aNotifier.mStepCount;
		mAssignedStepCount = aNotifier.mAssignedStepCount;
		mCurrentLevel = aNotifier.mCurrentLevel;
		return *this;
	}

	void
	increment(int aValue = 1)
	{
		PROGNOT_ASSERT(aValue > 0);
		PROGNOT_ASSERT(mStepCount > 0);

		checkCancelation();
		mAssignedStepCount += aValue;

		PROGNOT_ASSERT(mAssignedStepCount <= mStepCount);
		Progress currentProgress;

		{
			std::lock_guard<std::mutex> lock(mInfo->mtx);
			mInfo->progress += mSingleStep;
			currentProgress = mInfo->progress;
		}

		if (!mInfo->notifierImplementation->onIncrement(currentProgress)) {
			cancel();
		}
	}

	void
	cancel()
	{
		mInfo->canceled = true;
		checkCancelation();
	}

	void
	checkCancelation() {
		if (mInfo->canceled) {
			BOOST_THROW_EXCEPTION(CancelException());
		}
	}

	void
	setStepCount(int aStepCount)
	{
		PROGNOT_ASSERT(aStepCount > 0);
		mSingleStep = mProgressRange / aStepCount;
		mStepCount = aStepCount;
		mAssignedStepCount = 0;
	}

	ProgressNotifier
	subTaskNotifier(int aStepCount = 1)
	{
		mAssignedStepCount += aStepCount;
		return ProgressNotifier(mInfo, mSingleStep * aStepCount, mCurrentLevel + 1);
	}

protected:
	ProgressNotifier(
		std::shared_ptr<NotifierInfo> aInfo,
		Progress aProgressRange,
		int aCurrentLevel
		)
		: mInfo(aInfo)
		, mProgressRange(aProgressRange)
		, mCurrentLevel(aCurrentLevel)
	{}

	// noncopyable
	ProgressNotifier(ProgressNotifier &);
	ProgressNotifier &operator=(const ProgressNotifier &);


	//std::shared_ptr<AProgressNotifierImpl> mImpl;
	std::shared_ptr<NotifierInfo> mInfo;
	Progress mProgressRange;
	Progress mSingleStep;
	int mStepCount;
	int mAssignedStepCount;

	int mCurrentLevel;
};

template<typename TIterator>
class InputIteratorProgressNotifierAdaptor :
	public boost::iterator_adaptor<
		InputIteratorProgressNotifierAdaptor<TIterator>, // Derived
		TIterator,                                     // Base
		boost::use_default,                            // Value
		boost::single_pass_traversal_tag>              // CategoryOrTraversal
{
public:
	static const int64_t cStepCount = 100;
	InputIteratorProgressNotifierAdaptor()
		: InputIteratorProgressNotifierAdaptor::iterator_adaptor_(TIterator())
		, mPosition(-1)
	{}

	explicit InputIteratorProgressNotifierAdaptor(TIterator aIt, ProgressNotifier aProgressNotifier, int64_t aRangeSize)
		: InputIteratorProgressNotifierAdaptor::iterator_adaptor_(aIt)
		, mPosition(0)
		, mRangeSize(aRangeSize)
		, mProgressNotifier(std::make_shared<ProgressNotifier>(std::move(aProgressNotifier)))
	{
		mStepSize = (mRangeSize + cStepCount - 1)/ cStepCount;
		mProgressNotifier->setStepCount(cStepCount);
	}

private:
	friend class boost::iterator_core_access;
	void increment() {
		++this->base_reference();
		PROGNOT_ASSERT(mProgressNotifier);
		if ((++mPosition % mStepSize) == 0) {
			mProgressNotifier->increment(1);
		}
	}

	int64_t mPosition;
	int64_t mRangeSize;
	int64_t mStepSize;
	std::shared_ptr<ProgressNotifier> mProgressNotifier;
};

} // namespace prognot

