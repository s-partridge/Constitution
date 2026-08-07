#ifndef CGE_LISTENER_UNIT_TEST_H
#define CGE_LISTENER_UNIT_TEST_H

#include <partest/testbase.h>

namespace cge::test
{
	// Unit tests for listener.h. Everything here runs against a mock dispatcher,
	// so what is asserted is the listener's own behavior: what it hands to a
	// dispatcher, what it does with the answer, and how it dispatches an event
	// to the right handler once registration has been finalized.
	class ListenerUnitTest : public partest::TestBase
	{
	public:
		ListenerUnitTest();

	private:
		void returnsPending();
		void queuesOneCommand();
		void duplicateResult();
		void duplicateQueuesNothing();
		void refusedResult();
		void refusedRetry();
		void unregisterClearsPending();
		void unregisterUnknown();
		void reregisterAfterDrain();

		void handlerNotLiveYet();
		void invokesHandler();
		void passesPayload();
		void selectsByChannel();
		void ignoresUnknownChannel();
		void memberFunctionForm();
	};
}

#endif // CGE_LISTENER_UNIT_TEST_H
