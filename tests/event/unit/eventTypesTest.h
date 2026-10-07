#ifndef CGE_EVENT_TYPES_UNIT_TEST_H
#define CGE_EVENT_TYPES_UNIT_TEST_H

#include <partest/testbase.h>

namespace cge::test
{
	// Unit tests for eventTypes.h. The stream operator is what assertion
	// diagnostics print, so a wrong name there misreports every failure that
	// involves the status.
	class EventTypesUnitTest : public partest::TestBase
	{
	public:
		EventTypesUnitTest();

	private:
		void statusNames(partest::TestContext &ctx);
		void statusOutOfRange(partest::TestContext &ctx);
	};
}

#endif // CGE_EVENT_TYPES_UNIT_TEST_H
