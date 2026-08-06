#include "payloadTest.h"

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
	}
}
