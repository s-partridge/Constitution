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
		void broadcastQueuesOne();
		void broadcastChannel();
		void broadcastPayload();
		void broadcastCopies();
		void broadcastAccepted();
		void broadcastRefused();
		void broadcastRefusedQueue();
		void broadcastQueueOnly();

		void commandQueuesOne();
		void commandChannel();
		void commandPayload();
		void commandAccepted();
		void commandRefused();
		void commandRefusedQueue();
		void commandQueueOnly();
	};
}

#endif // CGE_BROADCASTER_UNIT_TEST_H
