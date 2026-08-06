#ifndef CGE_EVENT_LOAD_TEST_H
#define CGE_EVENT_LOAD_TEST_H

#include "eventTestSupport.h"

namespace cge::test
{
	// Game-shaped volume: multi-frame worker traffic with payload integrity
	// checks. Keeps a reference copy of every payload sent, collects everything
	// received, then compares as a multiset. Cross-worker total order is not
	// required and is deliberately not asserted.
	//
	// Slow relative to the rest of the suite. Frame-gated mode uses semaphores
	// so production only happens between drains, with no per-frame thread
	// create and join.
	class EventLoadTest : public EventTestBase
	{
	public:
		explicit EventLoadTest(const DispatcherFlavor &flavor);

	private:
		void frameGated();
		void continuous();

		void frameGatedWorkers();
		void frameGatedCascade();
		void frameGatedMixedEventAndCommand();

		void continuousWorkers();
		void continuousCascade();
	};
}

#endif // CGE_EVENT_LOAD_TEST_H
