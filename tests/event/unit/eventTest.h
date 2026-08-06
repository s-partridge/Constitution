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
		void eventStoresPayload();
		void eventCopiesPayloadAtConstruction();
		void eventOutlivesItsPayloadSource();
		void payloadCategoriesPreserved();

		void mismatchedPayloadTypeIsRefused();
		void sameNameGivesSameChannel();
		void distinctNamesGiveDistinctIds();
		void distinctRegistriesGiveDistinctChannels();
		void channelIsNotDefaultConstructible();
		void channelCopyKeepsId();
		void channelIsNotMoveConstructible();
	};
}

#endif // CGE_EVENT_UNIT_TEST_H
