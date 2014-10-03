#include "progress_notifier.hpp"
#include <iostream>
#include <iomanip>

using namespace prognot;

class TestNotifier: public AProgressNotifierImpl {
public:
	void
	onIncrement(Progress aProgress) override {
		std::cout << boost::rational_cast<double>(aProgress) * 100 << std::endl;
	}

	void
	onCancel() override {}

};

void
testFunction1(ProgressNotifier progress_notifier)
{
	progress_notifier.setStepCount(95);
	for (int i = 0; i < 95; ++i) {
		progress_notifier.increment();
	}
}

void
testFunction2(ProgressNotifier progress_notifier)
{
	progress_notifier.setStepCount(5);

	progress_notifier.increment();

	testFunction1(progress_notifier.subTaskNotifier(1));
	progress_notifier.increment();
	testFunction1(progress_notifier.subTaskNotifier(1));
	progress_notifier.increment();
}


int main(int argc, char **argv) {
	testFunction2(TestNotifier());
	return 0;
}
