#ifndef CGE_PAYLOAD_TEST_H
#define CGE_PAYLOAD_TEST_H

#include "eventTestSupport.h"

namespace cge::test
{
	// What survives the trip from broadcast to handler. Payload copying itself is
	// covered by the Event unit tests; what is left here is the end to end
	// delivery path, which only exists with a real dispatcher in it.
	class PayloadTest : public DispatcherFlavorSuite
	{
	public:
		explicit PayloadTest(const DispatcherFlavor &flavor);

	private:
		void delivery();
		void payloadTypes();
	};
}

#endif // CGE_PAYLOAD_TEST_H
