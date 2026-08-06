#include "payloadTest.h"

#include <string>
#include <vector>

#include <partest/assert.h>

#include "broadcaster.h"

namespace cge::test
{
	PayloadTest::PayloadTest(const DispatcherFlavor &flavor)
		: EventTestBase("PayloadTest", "Payload delivery and copy semantics.", flavor)
	{
		partest::TestFlags flags = partest::TEST_FLAGS_INHERIT;

		addTest("Delivery", flags, [this]() { delivery(); });
	}

	// One listener accumulating across the cases; each asserts against the
	// running total rather than rebuilding the fixture.
	void PayloadTest::delivery()
	{
		EventHarness harness(flavor(), "payload-delivery-dispatcher");
		const cge::event::EventChannel<int> &channel = harness.registry.getChannel<int>("bc-int");
		CountingListener listener(&harness.dispatcher());

		listener.requestRegister(channel, [&listener](const int &v) { listener.onInt(v); });
		harness.dispatcher().dispatchCommands();
		cge::event::BroadcasterBase broadcaster(&harness.dispatcher());

		subtest("TypedPayload", [&]() {
			broadcaster.broadcast(channel, 123);
			harness.dispatcher().dispatchEvents();

			ASSERT_EQUAL(listener.received.size(), static_cast<size_t>(1));
			ASSERT_EQUAL(listener.received[0], 123);
		});

		subtest("OrderPreserved", [&]() {
			broadcaster.broadcast(channel, 1);
			broadcaster.broadcast(channel, 2);
			broadcaster.broadcast(channel, 3);
			harness.dispatcher().dispatchEvents();

			ASSERT_EQUAL(listener.received.size(), static_cast<size_t>(4));
			ASSERT_EQUAL(listener.received[1], 1);
			ASSERT_EQUAL(listener.received[2], 2);
			ASSERT_EQUAL(listener.received[3], 3);
		});

		subtest("NoListeners", [&]() {
			const cge::event::EventChannel<int> &empty = harness.registry.getChannel<int>("bc-empty");

			ASSERT_NOTHROW(broadcaster.broadcast(empty, 1));
			ASSERT_NOTHROW(harness.dispatcher().dispatchEvents());
		});

		// The queue erases the payload type behind EventBase and the handler
		// casts back out of it. Event<T> holding a payload correctly is a unit
		// concern; what needs the real path is that the round trip through the
		// erased pointer returns the same value. An int would not show a shallow
		// copy or a lost buffer, so this uses a payload whose copy allocates.
		subtest("OwningPayloadSurvivesTheQueue", [&]() {
			struct SpawnRequest
			{
				int unitType;
				std::string name;
				std::vector<int> inventory;
			};

			const cge::event::EventChannel<SpawnRequest> &channel = harness.registry.getChannel<SpawnRequest>("bc-owning");
			SpawnRequest got;
			got.unitType = 0;

			cge::event::ListenerBase owner(&harness.dispatcher());
			owner.requestRegister(channel, [&got](const SpawnRequest &r) { got = r; });
			harness.dispatcher().dispatchCommands();

			SpawnRequest request;
			request.unitType = 3;
			request.name = "archer";
			request.inventory.push_back(10);
			request.inventory.push_back(20);

			broadcaster.broadcast(channel, request);

			// Source released before the drain, so anything shallow dangles.
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
