#include "eventTest.h"

#include <memory>
#include <string>
#include <type_traits>
#include <vector>

#include <partest/assert.h>

#include "event.h"

namespace cge::test
{
	namespace
	{
		enum class GameState { Menu, Loading, Playing };

		struct DamagePayload
		{
			int amount;
			float multiplier;
			unsigned sourceId;
		};

		struct SpawnRequest
		{
			int unitType;
			std::string name;
			std::vector<int> inventory;
		};
	}

	EventUnitTest::EventUnitTest()
		: TestBase("EventUnitTest", "Unit tests for event payloads, channels and the registry.")
	{
		partest::TestFlags flags = partest::TEST_FLAGS_INHERIT;

		addTest("EventStoresPayload", flags, [this]() { eventStoresPayload(); });
		addTest("EventCopiesPayloadAtConstruction", flags, [this]() { eventCopiesPayloadAtConstruction(); });
		addTest("EventOutlivesItsPayloadSource", flags, [this]() { eventOutlivesItsPayloadSource(); });
		addTest("EnumPayloadPreserved", flags, [this]() { enumPayloadPreserved(); });
		addTest("PointerPayloadPreservesAddress", flags, [this]() { pointerPayloadPreservesAddress(); });
		addTest("TrivialStructPayloadPreserved", flags, [this]() { trivialStructPayloadPreserved(); });
		addTest("ClassPayloadPreserved", flags, [this]() { classPayloadPreserved(); });
		addTest("AggregatePayloadPreserved", flags, [this]() { aggregatePayloadPreserved(); });

		addTest("SameNameGivesSameChannel", flags, [this]() { sameNameGivesSameChannel(); });
		addTest("DistinctNamesGiveDistinctIds", flags, [this]() { distinctNamesGiveDistinctIds(); });
		addTest("DistinctRegistriesGiveDistinctChannels", flags, [this]() { distinctRegistriesGiveDistinctChannels(); });
		addTest("ChannelIsNotDefaultConstructible", flags, [this]() { channelIsNotDefaultConstructible(); });
		addTest("ChannelCopyKeepsId", flags, [this]() { channelCopyKeepsId(); });
		addTest("ChannelIsNotMoveConstructible", flags, [this]() { channelIsNotMoveConstructible(); });
	}

	void EventUnitTest::eventStoresPayload()
	{
		cge::event::Event<int> event(42);

		ASSERT_EQUAL(event.payload, 42);
	}

	// The constructor takes a reference and stores a copy, so the caller owns
	// his source for as long as he likes and may change it immediately.
	void EventUnitTest::eventCopiesPayloadAtConstruction()
	{
		std::string source = "original";
		cge::event::Event<std::string> event(source);

		source = "mutated";

		ASSERT_EQUAL(event.payload, std::string("original"));
	}

	// The async case in miniature: the source is a local in a worker function
	// that returned long before anything reads the payload.
	void EventUnitTest::eventOutlivesItsPayloadSource()
	{
		std::unique_ptr<cge::event::Event<std::string>> event;
		{
			std::string source = "scoped";
			event = std::make_unique<cge::event::Event<std::string>>(source);
		}

		ASSERT_EQUAL(event->payload, std::string("scoped"));
	}

	void EventUnitTest::enumPayloadPreserved()
	{
		cge::event::Event<GameState> event(GameState::Playing);

		ASSERT_TRUE(event.payload == GameState::Playing);
	}

	// A pointer payload copies the address, not the pointee.
	void EventUnitTest::pointerPayloadPreservesAddress()
	{
		int target = 41;
		cge::event::Event<int *> event(&target);

		ASSERT_TRUE(event.payload == &target);

		*event.payload = 42;
		ASSERT_EQUAL(target, 42);
	}

	// The archetypal game payload.
	void EventUnitTest::trivialStructPayloadPreserved()
	{
		static_assert(std::is_trivially_copyable<DamagePayload>::value,
			"representative must actually belong to the trivially-copyable class");

		DamagePayload source;
		source.amount = 25;
		source.multiplier = 1.5f;
		source.sourceId = 7;

		cge::event::Event<DamagePayload> event(source);

		ASSERT_EQUAL(event.payload.amount, 25);
		ASSERT_EQUAL(event.payload.multiplier, 1.5f);
		ASSERT_EQUAL(event.payload.sourceId, 7u);
	}

	// Copy allocates, so this proves the deep copy rather than a shared buffer.
	void EventUnitTest::classPayloadPreserved()
	{
		cge::event::Event<std::string> event(std::string("hello-event"));

		ASSERT_EQUAL(event.payload, std::string("hello-event"));
	}

	// Members own resources, so the copy is member-wise and non-trivial.
	void EventUnitTest::aggregatePayloadPreserved()
	{
		SpawnRequest source;
		source.unitType = 3;
		source.name = "archer";
		source.inventory.push_back(10);
		source.inventory.push_back(20);

		cge::event::Event<SpawnRequest> event(source);
		source.name = "mutated";
		source.inventory.clear();

		ASSERT_EQUAL(event.payload.unitType, 3);
		ASSERT_EQUAL(event.payload.name, std::string("archer"));
		ASSERT_EQUAL(event.payload.inventory.size(), static_cast<size_t>(2));
		ASSERT_EQUAL(event.payload.inventory[0], 10);
		ASSERT_EQUAL(event.payload.inventory[1], 20);
	}

	void EventUnitTest::sameNameGivesSameChannel()
	{
		cge::event::EventChannelRegistry registry;
		const cge::event::EventChannel<int> &first = registry.getChannel<int>("alpha");
		const cge::event::EventChannel<int> &second = registry.getChannel<int>("alpha");

		ASSERT_EQUAL(first.id(), second.id());
		ASSERT_TRUE(&first == &second);
	}

	void EventUnitTest::distinctNamesGiveDistinctIds()
	{
		cge::event::EventChannelRegistry registry;
		const cge::event::EventChannel<int> &a = registry.getChannel<int>("a");
		const cge::event::EventChannel<int> &b = registry.getChannel<int>("b");

		ASSERT_NOT_EQUAL(a.id(), b.id());
	}

	// Channel ids come from a process-global counter, so the same tag in two
	// registries is two different channels rather than a collision.
	void EventUnitTest::distinctRegistriesGiveDistinctChannels()
	{
		cge::event::EventChannelRegistry first;
		cge::event::EventChannelRegistry second;

		const cge::event::EventChannel<int> &a = first.getChannel<int>("shared-name");
		const cge::event::EventChannel<int> &b = second.getChannel<int>("shared-name");

		ASSERT_NOT_EQUAL(a.id(), b.id());
	}

	// Protected default ctor: new channels must come from the registry.
	void EventUnitTest::channelIsNotDefaultConstructible()
	{
		ASSERT_FALSE(std::is_default_constructible<cge::event::EventChannel<int>>::value);
		ASSERT_FALSE(std::is_default_constructible<cge::event::EventChannelBase>::value);
	}

	void EventUnitTest::channelCopyKeepsId()
	{
		ASSERT_TRUE(std::is_copy_constructible<cge::event::EventChannel<int>>::value);

		cge::event::EventChannelRegistry registry;
		const cge::event::EventChannel<int> &original = registry.getChannel<int>("copyable");
		cge::event::EventChannel<int> copy(original);

		ASSERT_EQUAL(copy.id(), original.id());
	}

	// Move is disabled so channel identities cannot be shuffled past the registry.
	void EventUnitTest::channelIsNotMoveConstructible()
	{
		ASSERT_FALSE(std::is_move_constructible<cge::event::EventChannel<int>>::value);
		ASSERT_FALSE(std::is_move_constructible<cge::event::EventChannelBase>::value);
	}
}
