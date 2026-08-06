#ifndef CGE_ASYNC_DISPATCHER_UNIT_TEST_H
#define CGE_ASYNC_DISPATCHER_UNIT_TEST_H

#include <partest/testbase.h>

namespace cge::test
{
	// Unit tests for asyncDispatcher.h. Everything here drives the dispatcher
	// through its own protected push hooks, so no listener, broadcaster or
	// commander is involved and nothing depends on their behavior being correct.
	class AsyncDispatcherUnitTest : public partest::TestBase
	{
	public:
		AsyncDispatcherUnitTest();

	private:
		void newDispatcherIsInactive();
		void setUpActivates();
		void tearDownDeactivates();
		void setUpAfterTearDownReactivates();
		void repeatedSetUpIsIdempotent();
		void repeatedTearDownIsIdempotent();

		void pushWhileInactiveIsRefused();
		void pushWhileInactiveQueuesNothing();
		void eventPushLandsInEventQueue();
		void commandPushLandsInCommandQueue();
		void eventPushLeavesCommandQueueEmpty();
		void commandPushLeavesEventQueueEmpty();

		void dispatchOnEmptyQueuesIsSafe();
		void dispatchEventsDrainsWithNoListeners();
		void queuedEventsSurviveTearDown();
	};
}

#endif // CGE_ASYNC_DISPATCHER_UNIT_TEST_H
