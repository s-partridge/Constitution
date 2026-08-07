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
		void startsInactive();
		void setUpActivates();
		void tearDownDeactivates();
		void reactivates();
		void repeatedSetUp();
		void repeatedTearDown();

		void inactiveRefused();
		void inactiveQueuesNothing();
		void eventQueued();
		void commandQueued();
		void eventNotInCommands();
		void commandNotInEvents();

		void drainEmpty();
		void drainNoListeners();
		void queueSurvivesTearDown();
	};
}

#endif // CGE_ASYNC_DISPATCHER_UNIT_TEST_H
