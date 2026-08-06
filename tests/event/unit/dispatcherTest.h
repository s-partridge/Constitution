#ifndef CGE_DISPATCHER_UNIT_TEST_H
#define CGE_DISPATCHER_UNIT_TEST_H

#include <partest/testbase.h>

namespace cge::test
{
	// Unit tests for dispatcher.h. The subject is DispatcherBase itself, reached
	// through the mock that makes it concrete, so nothing here depends on the
	// async dispatcher's queueing policy. A real listener is used because the
	// listener map is what is under test and only a listener can occupy it.
	class DispatcherUnitTest : public partest::TestBase
	{
	public:
		DispatcherUnitTest();

	private:
		void registrationHasNoEffectBeforeCommandDrain();
		void registrationAppliesAtCommandDrain();
		void unregistrationAppliesAtCommandDrain();
		void unregisteringOneListenerLeavesOthers();
		void multipleListenersAllReceive();
		void registrationIsPerChannel();
		void eventOnUnwatchedChannelReachesNobody();
		void eventCarriesPayloadToHandler();
		void dispatchEventsEmptiesTheQueue();
		void dispatchCommandsEmptiesTheQueue();
	};
}

#endif // CGE_DISPATCHER_UNIT_TEST_H
