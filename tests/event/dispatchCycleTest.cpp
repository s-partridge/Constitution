#include "dispatchCycleTest.h"

#include <atomic>

#include <partest/assert.h>

#include "broadcaster.h"

namespace cge::test
{
	DispatchCycleTest::DispatchCycleTest(const DispatcherFlavor &flavor)
		: EventTestBase("DispatchCycleTest", "Deferral, cycle order, and mid-drain behavior.", flavor)
	{
		partest::TestFlags flags = partest::TEST_FLAGS_INHERIT;

		addTest("Deferral", flags, [this]() { deferral(); });
		addTest("Drain", flags, [this]() { drain(); });
		addTest("MidDrainRegistration", flags, [this]() { midDrainRegistration(); });
	}

	// One listener carried from request through to delivery: each case runs
	// against the dispatcher state the previous one left behind.
	void DispatchCycleTest::deferral()
	{
		EventHarness harness(flavor(), "deferral-dispatcher");
		const cge::event::EventChannel<int> &channel = harness.registry.getChannel<int>("deferral");
		CountingListener listener(&harness.dispatcher());
		cge::event::BroadcasterBase broadcaster(&harness.dispatcher());

		subtest("EmptyQueuesNoOp", [&]() {
			ASSERT_NOTHROW(harness.dispatcher().dispatchCommands());
			ASSERT_NOTHROW(harness.dispatcher().dispatchEvents());
		});

		subtest("RegistrationWaitsForCommandDrain", [&]() {
			ASSERT_TRUE(listener.requestRegister(channel, [&listener](const int &v) { listener.onInt(v); })
				== cge::event::RegistrationResult::Pending);

			// Listener map not updated yet, so this event drains to nobody.
			broadcaster.broadcast(channel, 7);
			harness.dispatcher().dispatchEvents();
			ASSERT_EQUAL(listener.received.size(), static_cast<size_t>(0));
		});

		subtest("EventsWaitForEventDrain", [&]() {
			harness.dispatcher().dispatchCommands();

			broadcaster.broadcast(channel, 42);
			ASSERT_EQUAL(listener.received.size(), static_cast<size_t>(0));

			harness.dispatcher().dispatchEvents();
			ASSERT_EQUAL(listener.received.size(), static_cast<size_t>(1));
			ASSERT_EQUAL(listener.received[0], 42);
		});
	}

	// Shared dispatcher; what varies is what the handler does during the drain.
	void DispatchCycleTest::drain()
	{
		EventHarness harness(flavor(), "drain-dispatcher");
		cge::event::BroadcasterBase broadcaster(&harness.dispatcher());

		subtest("ReentryBroadcasts", [&]() {
			const cge::event::EventChannel<int> &channel = harness.registry.getChannel<int>("reentry");
			CountingListener listener(&harness.dispatcher());

			listener.requestRegister(channel, [&](const int &v) {
				listener.onInt(v);
				if(v == 1)
					broadcaster.broadcast(channel, 2);
			});
			harness.dispatcher().dispatchCommands();

			broadcaster.broadcast(channel, 1);
			harness.dispatcher().dispatchEvents();

			ASSERT_EQUAL(listener.received.size(), static_cast<size_t>(2));
			ASSERT_EQUAL(listener.received[0], 1);
			ASSERT_EQUAL(listener.received[1], 2);
		});

		// Pending semantics: an unregistration requested mid-drain takes effect at
		// the next command drain, so the rest of this drain still delivers.
		subtest("UnregisterMidDrain", [&]() {
			const cge::event::EventChannel<int> &channel = harness.registry.getChannel<int>("same-drain");
			CountingListener listener(&harness.dispatcher());

			listener.requestRegister(channel, [&](const int &v) {
				listener.onInt(v);
				if(v == 1)
					listener.requestUnregister(channel);
			});
			harness.dispatcher().dispatchCommands();

			broadcaster.broadcast(channel, 1);
			broadcaster.broadcast(channel, 2);
			broadcaster.broadcast(channel, 3);
			harness.dispatcher().dispatchEvents();
			ASSERT_EQUAL(listener.received.size(), static_cast<size_t>(3));

			harness.dispatcher().dispatchCommands();
			broadcaster.broadcast(channel, 4);
			harness.dispatcher().dispatchEvents();
			ASSERT_EQUAL(listener.received.size(), static_cast<size_t>(3));
		});
	}

	// A handler registering another listener while the event drain is running.
	// Deterministic and single-threaded despite having lived in the load suite:
	// nothing here needs volume to reproduce.
	void DispatchCycleTest::midDrainRegistration()
	{
		subtest("TakesEffectAtNextCommandDrain", [&]() {
			EventHarness harness(flavor(), "mid-drain-reg-dispatcher");
			const cge::event::EventChannel<int> &kick = harness.registry.getChannel<int>("reg-kick");
			const cge::event::EventChannel<int> &late = harness.registry.getChannel<int>("reg-late");
			std::atomic<int> lateHits(0);
			std::atomic<bool> requested(false);

			cge::event::BroadcasterBase broadcaster(&harness.dispatcher());
			cge::event::ListenerBase kickListener(&harness.dispatcher());
			cge::event::ListenerBase lateListener(&harness.dispatcher());

			kickListener.requestRegister(kick, [&](const int &) {
				if(!requested.exchange(true))
				{
					lateListener.requestRegister(late, [&lateHits](const int &) {
						lateHits.fetch_add(1);
					});
				}
			});
			harness.dispatcher().dispatchCommands();

			broadcaster.broadcast(kick, 1);
			harness.dispatcher().dispatchEvents();
			ASSERT_EQUAL(lateHits.load(), 0);

			harness.dispatcher().dispatchCommands();
			broadcaster.broadcast(late, 99);
			harness.dispatcher().dispatchEvents();

			ASSERT_EQUAL(lateHits.load(), 1);
		});
	}
}
