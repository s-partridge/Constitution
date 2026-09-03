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
		void startsInactive(partest::TestContext &ctx);
		void setUpActivates(partest::TestContext &ctx);
		void tearDownDeactivates(partest::TestContext &ctx);
		void reactivates(partest::TestContext &ctx);
		void repeatedSetUp(partest::TestContext &ctx);
		void repeatedTearDown(partest::TestContext &ctx);

		void inactiveRefused(partest::TestContext &ctx);
		void inactiveQueuesNothing(partest::TestContext &ctx);
		void inactiveUnregisterQueued(partest::TestContext &ctx);
		void eventQueued(partest::TestContext &ctx);
		void commandQueued(partest::TestContext &ctx);
		void eventNotInCommands(partest::TestContext &ctx);
		void commandNotInEvents(partest::TestContext &ctx);

		void drainEmpty(partest::TestContext &ctx);
		void drainNoListeners(partest::TestContext &ctx);
		void queueSurvivesTearDown(partest::TestContext &ctx);
	};
}

#endif // CGE_ASYNC_DISPATCHER_UNIT_TEST_H
