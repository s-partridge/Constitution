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
		void storesPayload(partest::TestContext &ctx);
		void copiesPayload(partest::TestContext &ctx);
		void outlivesSource(partest::TestContext &ctx);
		void payloadCategories(partest::TestContext &ctx);
		void payloadsCopyable(partest::TestContext &ctx);
		void moveOnlyPayloadRejected(partest::TestContext &ctx);

		void typeConflict(partest::TestContext &ctx);
		void tryGetCreates(partest::TestContext &ctx);
		void tryGetMatchesGet(partest::TestContext &ctx);
		void sameName(partest::TestContext &ctx);
		void distinctNames(partest::TestContext &ctx);
		void distinctRegistries(partest::TestContext &ctx);
		void noDefaultConstruct(partest::TestContext &ctx);
		void copyKeepsId(partest::TestContext &ctx);
		void noMoveConstruct(partest::TestContext &ctx);
		void registryMove(partest::TestContext &ctx);
	};
}

#endif // CGE_EVENT_UNIT_TEST_H
