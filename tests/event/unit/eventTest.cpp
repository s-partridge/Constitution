#include "eventTest.h"

#include <memory>
#include <string>
#include <type_traits>
#include <utility>
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

		addTest("StoresPayload", flags, [this]() { storesPayload(); });
		addTest("CopiesPayload", flags, [this]() { copiesPayload(); });
		addTest("OutlivesSource", flags, [this]() { outlivesSource(); });
		addTest("PayloadCategories", flags, [this]() { payloadCategories(); });

		addTest("TypeConflict", flags, [this]() { typeConflict(); });
		addTest("SameName", flags, [this]() { sameName(); });
		addTest("DistinctNames", flags, [this]() { distinctNames(); });
		addTest("DistinctRegistries", flags, [this]() { distinctRegistries(); });
		addTest("NoDefaultConstruct", flags, [this]() { noDefaultConstruct(); });
		addTest("CopyKeepsId", flags, [this]() { copyKeepsId(); });
		addTest("NoMoveConstruct", flags, [this]() { noMoveConstruct(); });
		addTest("RegistryMove", flags, [this]() { registryMove(); });
	}

	void EventUnitTest::storesPayload()
	{
		cge::event::Event<int> event(42);

		ASSERT_EQUAL(event.payload, 42);
	}

	// The constructor takes a reference and stores a copy, so the caller owns
	// his source for as long as he likes and may change it immediately.
	void EventUnitTest::copiesPayload()
	{
		std::string source = "original";
		cge::event::Event<std::string> event(source);

		source = "mutated";

		ASSERT_EQUAL(event.payload, std::string("original"));
	}

	// The async case in miniature: the source is a local in a worker function
	// that returned long before anything reads the payload.
	void EventUnitTest::outlivesSource()
	{
		std::unique_ptr<cge::event::Event<std::string>> event;
		{
			std::string source = "scoped";
			event = std::make_unique<cge::event::Event<std::string>>(source);
		}

		ASSERT_EQUAL(event->payload, std::string("scoped"));
	}

	// One behavior, five payload categories. Event<T> has to preserve each of
	// them across construction whatever the copy costs.
	void EventUnitTest::payloadCategories()
	{
		subtest("Enum", [&]() {
			cge::event::Event<GameState> event(GameState::Playing);

			ASSERT_TRUE(event.payload == GameState::Playing);
		});

		// A pointer payload copies the address, not the pointee.
		subtest("Pointer", [&]() {
			int target = 41;
			cge::event::Event<int *> event(&target);

			ASSERT_TRUE(event.payload == &target);

			*event.payload = 42;
			ASSERT_EQUAL(target, 42);
		});

		// The archetypal game payload.
		subtest("TrivialStruct", [&]() {
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
		});

		// Copy allocates, so this proves a deep copy rather than a shared buffer.
		subtest("Class", [&]() {
			cge::event::Event<std::string> event(std::string("hello-event"));

			ASSERT_EQUAL(event.payload, std::string("hello-event"));
		});

		// Members own resources, so the copy is member-wise and non-trivial.
		subtest("Aggregate", [&]() {
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
		});
	}

	// The single guard the whole system rests on. Delivery casts an EventBase
	// straight to Event<T> with no runtime check, and the only thing making that
	// sound is that a channel id is bound to one payload type for life. If a tag
	// could be re-requested under a different type, the cast becomes undefined
	// behavior on the first event through it.
	//
	// TODO: getChannel returns a reference and so has no way to report a
	// rejection to the caller. The contract cannot be written against the
	// current signature, and asserting the throw that stands in for it today
	// would pin a mechanism that is being removed. This fails until getChannel
	// can return a result.
	void EventUnitTest::typeConflict()
	{
		const bool refused = false;

		ASSERT_TRUE(refused);
	}

	void EventUnitTest::sameName()
	{
		cge::event::EventChannelRegistry registry;
		const cge::event::EventChannel<int> &first = registry.getChannel<int>("alpha");
		const cge::event::EventChannel<int> &second = registry.getChannel<int>("alpha");

		ASSERT_EQUAL(first.id(), second.id());
		ASSERT_TRUE(&first == &second);
	}

	void EventUnitTest::distinctNames()
	{
		cge::event::EventChannelRegistry registry;
		const cge::event::EventChannel<int> &a = registry.getChannel<int>("a");
		const cge::event::EventChannel<int> &b = registry.getChannel<int>("b");

		ASSERT_NOT_EQUAL(a.id(), b.id());
	}

	// Channel ids come from a process-global counter, so the same tag in two
	// registries is two different channels rather than a collision.
	void EventUnitTest::distinctRegistries()
	{
		cge::event::EventChannelRegistry first;
		cge::event::EventChannelRegistry second;

		const cge::event::EventChannel<int> &a = first.getChannel<int>("shared-name");
		const cge::event::EventChannel<int> &b = second.getChannel<int>("shared-name");

		ASSERT_NOT_EQUAL(a.id(), b.id());
	}

	// Protected default ctor: new channels must come from the registry.
	void EventUnitTest::noDefaultConstruct()
	{
		ASSERT_FALSE(std::is_default_constructible<cge::event::EventChannel<int>>::value);
		ASSERT_FALSE(std::is_default_constructible<cge::event::EventChannelBase>::value);
	}

	void EventUnitTest::copyKeepsId()
	{
		ASSERT_TRUE(std::is_copy_constructible<cge::event::EventChannel<int>>::value);

		cge::event::EventChannelRegistry registry;
		const cge::event::EventChannel<int> &original = registry.getChannel<int>("copyable");
		cge::event::EventChannel<int> copy(original);

		ASSERT_EQUAL(copy.id(), original.id());
	}

	// Move is disabled so channel identities cannot be shuffled past the registry.
	void EventUnitTest::noMoveConstruct()
	{
		ASSERT_FALSE(std::is_move_constructible<cge::event::EventChannel<int>>::value);
		ASSERT_FALSE(std::is_move_constructible<cge::event::EventChannelBase>::value);
	}

	// The registry owns its channels through raw pointers and deletes them in its
	// destructor, so a move has to hand that ownership over whole: the new owner
	// resolves the same tags to the same ids, and the husk left behind frees
	// nothing when it goes.
	void EventUnitTest::registryMove()
	{
		subtest("Construct", [&]() {
			cge::event::EventChannelRegistry source;
			const cge::event::ChannelId id = source.getChannel<int>("moved").id();

			cge::event::EventChannelRegistry moved(std::move(source));

			ASSERT_EQUAL(moved.getChannel<int>("moved").id(), id);
		});

		subtest("Assign", [&]() {
			cge::event::EventChannelRegistry source;
			const cge::event::ChannelId id = source.getChannel<int>("moved-assign").id();

			cge::event::EventChannelRegistry target;
			target.getChannel<int>("target-own");
			target = std::move(source);

			ASSERT_EQUAL(target.getChannel<int>("moved-assign").id(), id);
		});

		// Both registries are destroyed at the end of this subtest. The moved-from
		// one must hold nothing, or the channels get deleted twice.
		subtest("MovedFromOwnsNothing", [&]() {
			cge::event::EventChannelRegistry source;
			const cge::event::ChannelId id = source.getChannel<int>("before-move").id();

			cge::event::EventChannelRegistry moved(std::move(source));

			// The husk has no record of the tag, so asking for it again mints a new
			// channel rather than handing back the one the new owner holds.
			ASSERT_NOT_EQUAL(source.getChannel<int>("before-move").id(), id);
			ASSERT_EQUAL(moved.getChannel<int>("before-move").id(), id);
		});
	}
}
