#ifndef CGE_LISTENER_REGISTRATION_TEST_H
#define CGE_LISTENER_REGISTRATION_TEST_H

#include "eventTestSupport.h"

namespace cge::test
{
	// Registration lifecycle, batched requests, unregistration and handler
	// forms. Base contracts, so this runs once per dispatcher flavor.
	class ListenerRegistrationTest : public DispatcherFlavorSuite
	{
	public:
		explicit ListenerRegistrationTest(const DispatcherFlavor &flavor);

	private:
		void registrationLifecycle(partest::TestContext &ctx);
		void unregister(partest::TestContext &ctx);
		void batchedRequests(partest::TestContext &ctx);
		void oneHandlerPerChannel(partest::TestContext &ctx);
		void handlers(partest::TestContext &ctx);
	};
}

#endif // CGE_LISTENER_REGISTRATION_TEST_H
