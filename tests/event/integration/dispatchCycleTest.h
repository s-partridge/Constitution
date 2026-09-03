#ifndef CGE_DISPATCH_CYCLE_TEST_H
#define CGE_DISPATCH_CYCLE_TEST_H

#include "eventTestSupport.h"

namespace cge::test
{
	// When queued work takes effect: deferral to the right drain, cycle order,
	// and what a handler may do while a drain is in progress. The heart of the
	// dispatcher contract, so this runs once per flavor.
	class DispatchCycleTest : public DispatcherFlavorSuite
	{
	public:
		explicit DispatchCycleTest(const DispatcherFlavor &flavor);

	private:
		void deferral(partest::TestContext &ctx);
		void drain(partest::TestContext &ctx);
		void midDrainRegistration(partest::TestContext &ctx);
		void frameCycle(partest::TestContext &ctx);
		void cascade(partest::TestContext &ctx);
		void cascadeAcrossChannels(partest::TestContext &ctx);
		void cascadeSelfReferential(partest::TestContext &ctx);
		void cascadeThroughSystems(partest::TestContext &ctx);
		void midDrainMutation(partest::TestContext &ctx);
	};
}

#endif // CGE_DISPATCH_CYCLE_TEST_H
