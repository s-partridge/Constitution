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
		void returnsPending(partest::TestContext &ctx);
		void queuesOneCommand(partest::TestContext &ctx);
		void duplicatePending(partest::TestContext &ctx);
		void duplicateQueuesCommand(partest::TestContext &ctx);
		void duplicateKeepsFirst(partest::TestContext &ctx);
		void refusedResult(partest::TestContext &ctx);
		void refusedRetry(partest::TestContext &ctx);
		void refusedUnregister(partest::TestContext &ctx);
		void registerAfterUnregister(partest::TestContext &ctx);
		void unregisterUnknown(partest::TestContext &ctx);
		void duplicateReported(partest::TestContext &ctx);
		void unregisterUnknownReported(partest::TestContext &ctx);
		void successNotReported(partest::TestContext &ctx);
		void reregisterAfterDrain(partest::TestContext &ctx);

		void handlerNotLiveYet(partest::TestContext &ctx);
		void invokesHandler(partest::TestContext &ctx);
		void passesPayload(partest::TestContext &ctx);
		void selectsByChannel(partest::TestContext &ctx);
		void selectsAcrossTypes(partest::TestContext &ctx);
		void ignoresUnknownChannel(partest::TestContext &ctx);
		void memberFunctionForm(partest::TestContext &ctx);
	};
}

#endif // CGE_LISTENER_UNIT_TEST_H
