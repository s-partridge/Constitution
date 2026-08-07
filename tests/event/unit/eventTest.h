#ifndef CGE_EVENT_UNIT_TEST_H
#define CGE_EVENT_UNIT_TEST_H

#include <partest/testbase.h>

namespace cge::test
{
	// Unit tests for event.h: the payload wrapper, channel identity and
	// construction policy, and the registry. No dispatcher is involved in any
	// of it, which is the point - none of this needs one.
	class EventUnitTest : public partest::TestBase
	{
	public:
		EventUnitTest();

	private:
		void storesPayload();
		void copiesPayload();
		void outlivesSource();
		void payloadCategories();

		void typeConflict();
		void sameName();
		void distinctNames();
		void distinctRegistries();
		void noDefaultConstruct();
		void copyKeepsId();
		void noMoveConstruct();
		void registryMove();
	};
}

#endif // CGE_EVENT_UNIT_TEST_H
