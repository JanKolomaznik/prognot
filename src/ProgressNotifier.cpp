#include <prognot/Qt/ProgressNotifier.hpp>

#include "moc_ProgressNotifier.cpp"

namespace prognot {
namespace qt {

bool
ProgressNotifier::onIncrement(Progress aProgress)
{
	emit incremented(ceil(boost::rational_cast<float>(aProgress) * 100));
	return true;
}

} // namespace qt
} // namespace prognot
