#ifndef CGE_DISPATCHER_LIFECYCLE_TEST_H
#define CGE_DISPATCHER_LIFECYCLE_TEST_H

#include "eventTestSupport.h"

namespace cge::test
{
	// Setup, teardown, and what intake does on either side of them. The push
	// result cases live here rather than with the broadcaster and commander
	// suites because refusal while inactive is one contract, not three.
	class DispatcherLifecycleTest : public DispatcherFlavorSuite
	{
	public:
		explicit DispatcherLifecycleTest(const DispatcherFlavor &flavor);

	private:
		void lifecycle(partest::TestContext &ctx);
		void restoration(partest::TestContext &ctx);
		void eviction(partest::TestContext &ctx);
		void destruction(partest::TestContext &ctx);
		void broadcastPushResult(partest::TestContext &ctx);
		void commandPushResult(partest::TestContext &ctx);
	};
}

#endif // CGE_DISPATCHER_LIFECYCLE_TEST_H
