#ifndef CGE_COMMANDER_TEST_H
#define CGE_COMMANDER_TEST_H

#include "eventTestSupport.h"

namespace cge::test
{
	// Command channel semantics. Thin until a command vocabulary exists that a
	// caller can legitimately send: today the only accepted commands are
	// registration and unregistration, and those originate in ListenerBase.
	class CommanderTest : public DispatcherFlavorSuite
	{
	public:
		explicit CommanderTest(const DispatcherFlavor &flavor);

	private:
		void command();
		void nonRegistrationChannel();
	};
}

#endif // CGE_COMMANDER_TEST_H
