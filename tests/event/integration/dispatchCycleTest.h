#ifndef CGE_DISPATCH_CYCLE_TEST_H
#define CGE_DISPATCH_CYCLE_TEST_H

#include "eventTestSupport.h"

namespace cge::test
{
	// When queued work takes effect: deferral to the right drain, cycle order,
	// and what a handler may do while a drain is in progress. The heart of the
	// dispatcher contract, so this runs once per flavor.
	class DispatchCycleTest : public EventTestBase
	{
	public:
		explicit DispatchCycleTest(const DispatcherFlavor &flavor);

	private:
		void deferral();
		void drain();
		void midDrainRegistration();
		void frameCycle();
	};
}

#endif // CGE_DISPATCH_CYCLE_TEST_H
