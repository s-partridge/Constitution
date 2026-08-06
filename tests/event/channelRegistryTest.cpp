#include "channelRegistryTest.h"

#include <type_traits>

#include <partest/assert.h>

#include "event.h"

namespace cge::test
{
	ChannelRegistryTest::ChannelRegistryTest()
		: TestBase("ChannelRegistryTest", "Channel registry creation and construction policy.")
	{
		partest::TestFlags flags = partest::TEST_FLAGS_INHERIT;

		addTest("Lookup", flags, [this]() { lookup(); });
		addTest("ConstructionPolicy", flags, [this]() { constructionPolicy(); });
	}

	void ChannelRegistryTest::lookup()
	{
		subtest("SameNameSameInstance", [&]() {
			cge::event::EventChannelRegistry registry;
			const cge::event::EventChannel<int> &first = registry.getChannel<int>("alpha");
			const cge::event::EventChannel<int> &second = registry.getChannel<int>("alpha");

			ASSERT_EQUAL(first.id(), second.id());
			ASSERT_TRUE(&first == &second);
		});

		subtest("DistinctNamesDistinctIds", [&]() {
			cge::event::EventChannelRegistry registry;
			const cge::event::EventChannel<int> &a = registry.getChannel<int>("a");
			const cge::event::EventChannel<int> &b = registry.getChannel<int>("b");

			ASSERT_NOT_EQUAL(a.id(), b.id());
		});
	}

	void ChannelRegistryTest::constructionPolicy()
	{
		// Protected default ctor: new channels must come from the registry.
		subtest("NotDefaultConstructible", [&]() {
			ASSERT_FALSE(std::is_default_constructible<cge::event::EventChannel<int>>::value);
			ASSERT_FALSE(std::is_default_constructible<cge::event::EventChannelBase>::value);
		});

		subtest("CopyKeepsId", [&]() {
			ASSERT_TRUE(std::is_copy_constructible<cge::event::EventChannel<int>>::value);

			cge::event::EventChannelRegistry registry;
			const cge::event::EventChannel<int> &original = registry.getChannel<int>("copyable");
			cge::event::EventChannel<int> copy(original);

			ASSERT_EQUAL(copy.id(), original.id());
		});

		// Move is disabled so channel identities cannot be shuffled past the registry.
		subtest("NotMoveConstructible", [&]() {
			ASSERT_FALSE(std::is_move_constructible<cge::event::EventChannel<int>>::value);
			ASSERT_FALSE(std::is_move_constructible<cge::event::EventChannelBase>::value);
		});
	}
}
