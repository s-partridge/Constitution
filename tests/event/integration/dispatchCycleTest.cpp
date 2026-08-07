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
		addTest("FrameCycle", flags, [this]() { frameCycle(); });
	}

	// One listener carried from request through to delivery: each case runs
	// against the dispatcher state the previous one left behind.
	void DispatchCycleTest::deferral()
	{
		EventHarness harness(flavor(), "deferral-dispatcher");
		const cge::event::EventChannel<int> &channel = harness.registry.getChannel<int>("deferral");
		CountingListener listener(&harness.dispatcher());
		cge::event::BroadcasterBase broadcaster(&harness.dispatcher());

		subtest("RegistrationWaitsForCommandDrain", [&]() {
			listener.requestRegister(channel, [&listener](const int &v) { listener.onInt(v); });

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

			// A count alone passes on a re-delivery bug producing 1, 1, 2, so the
			// values are what the contract is actually stated against.
			ASSERT_EQUAL(listener.received.size(), static_cast<size_t>(3));
			if(listener.received.size() == 3)
			{
				ASSERT_EQUAL(listener.received[0], 1);
				ASSERT_EQUAL(listener.received[1], 2);
				ASSERT_EQUAL(listener.received[2], 3);
			}

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

	// What a whole frame settles, rather than what half of one does. Every case
	// here runs commands, events, commands, and then asks its question with an
	// event drain alone: anything a handler requested during the drain has to
	// have been applied by the trailing command pass, not be waiting on the next
	// frame's leading one.
	void DispatchCycleTest::frameCycle()
	{
		subtest("UnregisterAppliesWithinTheFrame", [&]() {
			EventHarness harness(flavor(), "frame-unreg-dispatcher");
			const cge::event::EventChannel<int> &channel = harness.registry.getChannel<int>("frame-unreg");
			CountingListener listener(&harness.dispatcher());
			cge::event::BroadcasterBase broadcaster(&harness.dispatcher());

			listener.requestRegister(channel, [&](const int &v) {
				listener.onInt(v);
				if(v == 1)
					listener.requestUnregister(channel);
			});
			frame(harness.dispatcher());

			broadcaster.broadcast(channel, 1);
			broadcaster.broadcast(channel, 2);
			frame(harness.dispatcher());
			ASSERT_EQUAL(listener.received.size(), static_cast<size_t>(2));

			// No leading command drain, so a delivery here would mean the
			// unregistration was still sitting in the queue when the frame ended.
			broadcaster.broadcast(channel, 3);
			harness.dispatcher().dispatchEvents();
			ASSERT_EQUAL(listener.received.size(), static_cast<size_t>(2));
		});

		subtest("RegisterAppliesWithinTheFrame", [&]() {
			EventHarness harness(flavor(), "frame-reg-dispatcher");
			const cge::event::EventChannel<int> &kick = harness.registry.getChannel<int>("frame-kick");
			const cge::event::EventChannel<int> &late = harness.registry.getChannel<int>("frame-late");
			cge::event::BroadcasterBase broadcaster(&harness.dispatcher());
			cge::event::ListenerBase kickListener(&harness.dispatcher());
			CountingListener lateListener(&harness.dispatcher());
			bool requested = false;

			kickListener.requestRegister(kick, [&](const int &) {
				if(requested)
					return;

				requested = true;
				lateListener.requestRegister(late, [&lateListener](const int &v) {
					lateListener.onInt(v);
				});
			});
			frame(harness.dispatcher());

			broadcaster.broadcast(kick, 1);
			frame(harness.dispatcher());

			broadcaster.broadcast(late, 99);
			harness.dispatcher().dispatchEvents();

			ASSERT_EQUAL(lateListener.received.size(), static_cast<size_t>(1));
			if(lateListener.received.size() == 1)
				ASSERT_EQUAL(lateListener.received[0], 99);
		});

		// The ordering the leading command drain exists for: a request made after
		// the events were broadcast still beats them, because commands run first.
		subtest("CommandsLeadTheFrame", [&]() {
			EventHarness harness(flavor(), "frame-order-dispatcher");
			const cge::event::EventChannel<int> &channel = harness.registry.getChannel<int>("frame-order");
			CountingListener listener(&harness.dispatcher());
			cge::event::BroadcasterBase broadcaster(&harness.dispatcher());

			listener.requestRegister(channel, [&listener](const int &v) { listener.onInt(v); });
			frame(harness.dispatcher());

			broadcaster.broadcast(channel, 1);
			listener.requestUnregister(channel);
			frame(harness.dispatcher());

			ASSERT_EQUAL(listener.received.size(), static_cast<size_t>(0));
		});
	}
}
