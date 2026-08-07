#include "payloadTest.h"

#include <string>
#include <vector>

#include <partest/assert.h>

#include "broadcaster.h"

namespace cge::test
{
	PayloadTest::PayloadTest(const DispatcherFlavor &flavor)
		: DispatcherFlavorSuite("PayloadTest", "Payload delivery and copy semantics.", flavor)
	{
		partest::TestFlags flags = partest::TEST_FLAGS_INHERIT;

		addTest("Delivery", flags, [this]() { delivery(); });
		addTest("PayloadTypes", flags, [this]() { payloadTypes(); });
	}

	// The dispatcher is shared, but each case brings its own channel and its own
	// listener, so no case depends on what a previous one left in the log.
	void PayloadTest::delivery()
	{
		EventHarness harness(flavor(), "payload-delivery-dispatcher");
		cge::event::BroadcasterBase broadcaster(&harness.dispatcher());

		subtest("TypedPayload", [&]() {
			const cge::event::EventChannel<int> &channel = harness.registry.getChannel<int>("bc-typed");
			CountingListener listener(&harness.dispatcher());

			listener.requestRegister(channel, [&listener](const int &v) { listener.onInt(v); });
			harness.dispatcher().dispatchCommands();

			broadcaster.broadcast(channel, 123);
			harness.dispatcher().dispatchEvents();

			ASSERT_EQUAL(listener.received.size(), static_cast<size_t>(1));
			ASSERT_EQUAL(listener.received[0], 123);
		});

		subtest("OrderPreserved", [&]() {
			const cge::event::EventChannel<int> &channel = harness.registry.getChannel<int>("bc-order");
			CountingListener listener(&harness.dispatcher());

			listener.requestRegister(channel, [&listener](const int &v) { listener.onInt(v); });
			harness.dispatcher().dispatchCommands();

			broadcaster.broadcast(channel, 1);
			broadcaster.broadcast(channel, 2);
			broadcaster.broadcast(channel, 3);
			harness.dispatcher().dispatchEvents();

			ASSERT_EQUAL(listener.received.size(), static_cast<size_t>(3));
			ASSERT_EQUAL(listener.received[0], 1);
			ASSERT_EQUAL(listener.received[1], 2);
			ASSERT_EQUAL(listener.received[2], 3);
		});

		// A channel nobody watches is not a refusal, so the push is accepted and
		// the drain simply discards it. Nothing observes the event itself, so the
		// proof that the drain handled it is that the next delivery still works.
		subtest("NoListeners", [&]() {
			const cge::event::EventChannel<int> &empty = harness.registry.getChannel<int>("bc-empty");
			const cge::event::EventChannel<int> &watched = harness.registry.getChannel<int>("bc-after-empty");
			CountingListener listener(&harness.dispatcher());

			listener.requestRegister(watched, [&listener](const int &v) { listener.onInt(v); });
			harness.dispatcher().dispatchCommands();

			ASSERT_TRUE(broadcaster.broadcast(empty, 1));
			harness.dispatcher().dispatchEvents();

			broadcaster.broadcast(watched, 55);
			harness.dispatcher().dispatchEvents();

			ASSERT_EQUAL(listener.received.size(), static_cast<size_t>(1));
			ASSERT_EQUAL(listener.received[0], 55);
		});

		// Channel copies preserve the id, and the id is the whole of a channel's
		// identity as far as routing is concerned. Registering through the
		// registry's reference and broadcasting through a stack copy is the reason
		// that preservation matters.
		subtest("CopiedChannelRoutes", [&]() {
			const cge::event::EventChannel<int> &original = harness.registry.getChannel<int>("bc-copied");
			CountingListener listener(&harness.dispatcher());

			listener.requestRegister(original, [&listener](const int &v) { listener.onInt(v); });
			harness.dispatcher().dispatchCommands();

			cge::event::EventChannel<int> copy(original);
			broadcaster.broadcast(copy, 77);
			harness.dispatcher().dispatchEvents();

			ASSERT_EQUAL(listener.received.size(), static_cast<size_t>(1));
			if(listener.received.size() == 1)
				ASSERT_EQUAL(listener.received[0], 77);
		});
	}

	// The queue erases the payload type behind EventBase, and the handler casts
	// straight back out of it with no check. Every payload type compiles its own
	// cast at the delivery site, so each category is a separate path and each
	// one is exercised end to end here. Event<T> holding a value correctly is a
	// separate, unit-level concern and does not establish any of this.
	void PayloadTest::payloadTypes()
	{
		EventHarness harness(flavor(), "payload-types-dispatcher");
		cge::event::BroadcasterBase broadcaster(&harness.dispatcher());

		subtest("Enum", [&]() {
			enum class GameState { Menu, Loading, Playing };

			const cge::event::EventChannel<GameState> &channel = harness.registry.getChannel<GameState>("bc-enum");
			GameState got = GameState::Menu;

			cge::event::ListenerBase listener(&harness.dispatcher());
			listener.requestRegister(channel, [&got](const GameState &s) { got = s; });
			harness.dispatcher().dispatchCommands();

			broadcaster.broadcast(channel, GameState::Playing);
			harness.dispatcher().dispatchEvents();

			ASSERT_TRUE(got == GameState::Playing);
		});

		// The address must arrive intact, not the pointee.
		subtest("Pointer", [&]() {
			const cge::event::EventChannel<int *> &channel = harness.registry.getChannel<int *>("bc-ptr");
			int target = 41;
			int *got = nullptr;

			cge::event::ListenerBase listener(&harness.dispatcher());
			listener.requestRegister(channel, [&got](int *const &p) { got = p; });
			harness.dispatcher().dispatchCommands();

			broadcaster.broadcast(channel, &target);
			harness.dispatcher().dispatchEvents();

			ASSERT_TRUE(got == &target);
			*got = 42;
			ASSERT_EQUAL(target, 42);
		});

		subtest("TrivialStruct", [&]() {
			struct DamagePayload
			{
				int amount;
				float multiplier;
				unsigned sourceId;
			};

			const cge::event::EventChannel<DamagePayload> &channel = harness.registry.getChannel<DamagePayload>("bc-struct");
			DamagePayload got;
			got.amount = 0;
			got.multiplier = 0.0f;
			got.sourceId = 0;

			cge::event::ListenerBase listener(&harness.dispatcher());
			listener.requestRegister(channel, [&got](const DamagePayload &p) { got = p; });
			harness.dispatcher().dispatchCommands();

			DamagePayload payload;
			payload.amount = 25;
			payload.multiplier = 1.5f;
			payload.sourceId = 7;

			broadcaster.broadcast(channel, payload);
			harness.dispatcher().dispatchEvents();

			ASSERT_EQUAL(got.amount, 25);
			ASSERT_EQUAL(got.multiplier, 1.5f);
			ASSERT_EQUAL(got.sourceId, 7u);
		});

		// Source released before the drain, so a shallow copy dangles.
		subtest("Class", [&]() {
			const cge::event::EventChannel<std::string> &channel = harness.registry.getChannel<std::string>("bc-str");
			std::string got;

			cge::event::ListenerBase listener(&harness.dispatcher());
			listener.requestRegister(channel, [&got](const std::string &s) { got = s; });
			harness.dispatcher().dispatchCommands();

			std::string source = "hello-event";
			broadcaster.broadcast(channel, source);
			source.clear();

			harness.dispatcher().dispatchEvents();

			ASSERT_EQUAL(got, std::string("hello-event"));
		});

		// Members own resources, so the copy is member-wise and non-trivial.
		subtest("Aggregate", [&]() {
			struct SpawnRequest
			{
				int unitType;
				std::string name;
				std::vector<int> inventory;
			};

			const cge::event::EventChannel<SpawnRequest> &channel = harness.registry.getChannel<SpawnRequest>("bc-agg");
			SpawnRequest got;
			got.unitType = 0;

			cge::event::ListenerBase listener(&harness.dispatcher());
			listener.requestRegister(channel, [&got](const SpawnRequest &r) { got = r; });
			harness.dispatcher().dispatchCommands();

			SpawnRequest request;
			request.unitType = 3;
			request.name = "archer";
			request.inventory.push_back(10);
			request.inventory.push_back(20);

			broadcaster.broadcast(channel, request);
			request.name.clear();
			request.inventory.clear();

			harness.dispatcher().dispatchEvents();

			ASSERT_EQUAL(got.unitType, 3);
			ASSERT_EQUAL(got.name, std::string("archer"));
			ASSERT_EQUAL(got.inventory.size(), static_cast<size_t>(2));
			ASSERT_EQUAL(got.inventory[0], 10);
			ASSERT_EQUAL(got.inventory[1], 20);
		});
	}
}
