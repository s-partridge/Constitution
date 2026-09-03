#ifndef CGE_BROADCASTER_UNIT_TEST_H
#define CGE_BROADCASTER_UNIT_TEST_H

#include <partest/testbase.h>

namespace cge::test
{
	// Unit tests for broadcaster.h: BroadcasterBase and CommanderBase. The two
	// are the same class with a different destination queue, so the same set of
	// behaviors is asserted twice, once each.
	class BroadcasterUnitTest : public partest::TestBase
	{
	public:
		BroadcasterUnitTest();

	private:
		void broadcastQueuesOne(partest::TestContext &ctx);
		void broadcastChannel(partest::TestContext &ctx);
		void broadcastPayload(partest::TestContext &ctx);
		void broadcastCopies(partest::TestContext &ctx);
		void broadcastAccepted(partest::TestContext &ctx);
		void broadcastRefused(partest::TestContext &ctx);
		void broadcastRefusedQueue(partest::TestContext &ctx);
		void broadcastQueueOnly(partest::TestContext &ctx);

		void commandQueuesOne(partest::TestContext &ctx);
		void commandChannel(partest::TestContext &ctx);
		void commandPayload(partest::TestContext &ctx);
		void commandAccepted(partest::TestContext &ctx);
		void commandRefused(partest::TestContext &ctx);
		void commandRefusedQueue(partest::TestContext &ctx);
		void commandQueueOnly(partest::TestContext &ctx);
	};
}

#endif // CGE_BROADCASTER_UNIT_TEST_H
