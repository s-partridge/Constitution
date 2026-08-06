#ifndef CGE_PAYLOAD_TEST_H
#define CGE_PAYLOAD_TEST_H

#include "eventTestSupport.h"

namespace cge::test
{
	// What survives the trip from broadcast to handler. Payload copying is its
	// own contract surface and has nothing to do with dispatch timing, which is
	// why it no longer shares a suite with the push result cases.
	class PayloadTest : public EventTestBase
	{
	public:
		explicit PayloadTest(const DispatcherFlavor &flavor);

	private:
		void delivery();
		void payloadTypes();
	};
}

#endif // CGE_PAYLOAD_TEST_H
