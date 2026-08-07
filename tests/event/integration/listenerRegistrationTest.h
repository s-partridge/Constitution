#ifndef CGE_LISTENER_REGISTRATION_TEST_H
#define CGE_LISTENER_REGISTRATION_TEST_H

#include "eventTestSupport.h"

namespace cge::test
{
	// Registration lifecycle, batched requests, unregistration and handler
	// forms. Base contracts, so this runs once per dispatcher flavor.
	class ListenerRegistrationTest : public EventTestBase
	{
	public:
		explicit ListenerRegistrationTest(const DispatcherFlavor &flavor);

	private:
		void registrationLifecycle();
		void unregister();
		void batchedRequests();
		void oneHandlerPerChannel();
		void handlers();
	};
}

#endif // CGE_LISTENER_REGISTRATION_TEST_H
