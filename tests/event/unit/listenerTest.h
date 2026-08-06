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
		void requestRegisterReturnsPending();
		void requestRegisterQueuesOneCommand();
		void duplicateRequestReturnsDuplicate();
		void duplicateRequestQueuesNothing();
		void refusedRegistrationReturnsFailure();
		void refusedRegistrationLeavesListenerUsable();
		void unregisterClearsPendingRegistration();
		void unregisterOfUnknownChannelLeavesListenerUsable();
		void registeredListenerRejectsSecondRequest();

		void handlerNotInvokedBeforeFinalize();
		void onEventInvokesHandler();
		void onEventPassesPayload();
		void onEventSelectsTheChannelHandler();
		void onEventIgnoresUnknownChannel();
		void memberFunctionOverloadInvokesMember();
	};
}

#endif // CGE_LISTENER_UNIT_TEST_H
