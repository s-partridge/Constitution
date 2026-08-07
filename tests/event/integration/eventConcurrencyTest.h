#ifndef CGE_EVENT_CONCURRENCY_TEST_H
#define CGE_EVENT_CONCURRENCY_TEST_H

#include "eventTestSupport.h"

namespace cge::test
{
	// Correctness when pushes and registration requests arrive from threads
	// other than the one draining. Accepting work from any thread is a base
	// contract rather than an async-only trait, so this runs per flavor. Volume
	// runs live in the load suite.
	class EventConcurrencyTest : public EventLoadSuite
	{
	public:
		explicit EventConcurrencyTest(const DispatcherFlavor &flavor);

	private:
		void threads();
		void churn();
	};
}

#endif // CGE_EVENT_CONCURRENCY_TEST_H
