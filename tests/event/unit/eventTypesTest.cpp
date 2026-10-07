#include "eventTypesTest.h"

#include <sstream>
#include <string>

#include <partest/assert.h>

#include "eventTypes.h"

namespace cge::test
{
	namespace
	{
		struct NamedStatus
		{
			cge::event::DispatchStatus status;
			const char *name;
		};

		// One row per enumerator. C++ cannot enumerate an enum's values, so a new
		// status is only covered once a row is added here for it.
		const NamedStatus statusNameTable[] = {
			{ cge::event::DispatchStatus::Success, "Success" },
			{ cge::event::DispatchStatus::Failure, "Failure" },
			{ cge::event::DispatchStatus::Duplicate, "Duplicate" },
			{ cge::event::DispatchStatus::Pending, "Pending" },
			{ cge::event::DispatchStatus::NotReady, "NotReady" },
			{ cge::event::DispatchStatus::Invalid, "Invalid" },
			{ cge::event::DispatchStatus::BadInput, "BadInput" },
			{ cge::event::DispatchStatus::WrongThread, "WrongThread" },
		};

		std::string statusText(cge::event::DispatchStatus status)
		{
			std::ostringstream out;
			out << status;
			return out.str();
		}
	}

	EventTypesUnitTest::EventTypesUnitTest()
		: TestBase("EventTypesUnitTest", "Unit tests for DispatchStatus text output.")
	{
		partest::TestFlags flags = partest::TEST_FLAGS_INHERIT;

		addTest("StatusNames", flags, PARTEST_CTX(this) { statusNames(ctx); });
		addTest("StatusOutOfRange", flags, PARTEST_CTX(this) { statusOutOfRange(ctx); });
	}

	// Each enumerator prints its own name, reported per value so a failure
	// names the status that is wrong.
	void EventTypesUnitTest::statusNames(partest::TestContext &ctx)
	{
		for(const NamedStatus &entry : statusNameTable)
		{
			ctx.subtest(entry.name, PARTEST_CTX(&) {
				ASSERT_EQUAL(statusText(entry.status), std::string(entry.name));
			});
		}
	}

	// A value outside the enumerators still prints something recognizable
	// rather than nothing.
	void EventTypesUnitTest::statusOutOfRange(partest::TestContext &ctx)
	{
		const cge::event::DispatchStatus unknown = static_cast<cge::event::DispatchStatus>(255);

		ASSERT_EQUAL(statusText(unknown), std::string("BadValue"));
	}
}
