#include "dispatchCycleTest.h"

#include <vector>

#include <partest/assert.h>

#include "broadcaster.h"

namespace cge::test
{
	namespace
	{
		// The shape a game system actually takes: it hears one thing and emits
		// another. It owns a listener and a broadcaster rather than being both.
		//
		// Deriving from both compiles today, since the two share no ancestor, but
		// it is not the intended design. Listener and broadcaster are headed for
		// being components, and one component being both would mean two component
		// identities and two lifetimes in one object. The owner is whatever ends
		// up holding them - an actor, a system, something that does not exist
		// yet - so the test stands in for that rather than for the shortcut.
		struct RelayNode
		{
			cge::event::ListenerBase listener;
			cge::event::BroadcasterBase broadcaster;

			explicit RelayNode(cge::event::DispatcherBase *dispatcher)
				: listener(dispatcher)
				, broadcaster(dispatcher)
			{
			}
		};
	}

	DispatchCycleTest::DispatchCycleTest(const DispatcherFlavor &flavor)
		: DispatcherFlavorSuite("DispatchCycleTest", "Deferral, cycle order, and mid-drain behavior.", flavor)
	{
		partest::TestFlags flags = partest::TEST_FLAGS_INHERIT;

		addTest("Deferral", flags, [this]() { deferral(); });
		addTest("Drain", flags, [this]() { drain(); });
		addTest("MidDrainRegistration", flags, [this]() { midDrainRegistration(); });
		addTest("FrameCycle", flags, [this]() { frameCycle(); });
		addTest("Cascade", flags, [this]() { cascade(); });
		addTest("MidDrainMutation", flags, [this]() { midDrainMutation(); });
	}

	// One listener carried from request through to delivery: each case runs
	// against the dispatcher state the previous one left behind.
	void DispatchCycleTest::deferral()
	{
		EventHarness harness(flavor(), "deferral-dispatcher");
		const cge::event::EventChannel<int> &channel = harness.registry.getChannel<int>("deferral");
		CountingListener listener(&harness.dispatcher());
		cge::event::BroadcasterBase broadcaster(&harness.dispatcher());

		subtest("Registration", [&]() {
			listener.requestRegister(channel, [&listener](const int &v) { listener.onInt(v); });

			// Listener map not updated yet, so this event drains to nobody.
			broadcaster.broadcast(channel, 7);
			harness.dispatcher().dispatchEvents();
			ASSERT_EQUAL(listener.received.size(), static_cast<size_t>(0));
		});

		subtest("Events", [&]() {
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
		EventHarness harness(flavor(), "mid-drain-reg-dispatcher");
		const cge::event::EventChannel<int> &kick = harness.registry.getChannel<int>("reg-kick");
		const cge::event::EventChannel<int> &late = harness.registry.getChannel<int>("reg-late");
		int lateHits = 0;
		bool requested = false;

		cge::event::BroadcasterBase broadcaster(&harness.dispatcher());
		cge::event::ListenerBase kickListener(&harness.dispatcher());
		cge::event::ListenerBase lateListener(&harness.dispatcher());

		kickListener.requestRegister(kick, [&](const int &) {
			if(requested)
				return;

			requested = true;
			lateListener.requestRegister(late, [&lateHits](const int &) { ++lateHits; });
		});
		harness.dispatcher().dispatchCommands();

		broadcaster.broadcast(kick, 1);
		harness.dispatcher().dispatchEvents();
		ASSERT_EQUAL(lateHits, 0);

		harness.dispatcher().dispatchCommands();
		broadcaster.broadcast(late, 99);
		harness.dispatcher().dispatchEvents();

		ASSERT_EQUAL(lateHits, 1);
	}

	// What a whole frame settles, rather than what half of one does. Every case
	// here runs commands, events, commands, and then asks its question with an
	// event drain alone: anything a handler requested during the drain has to
	// have been applied by the trailing command pass, not be waiting on the next
	// frame's leading one.
	void DispatchCycleTest::frameCycle()
	{
		subtest("UnregisterApplies", [&]() {
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

		subtest("RegisterApplies", [&]() {
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
		subtest("CommandsLead", [&]() {
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

	// The shape ordinary game code produces: damage kills something, the death
	// scores, the score unlocks an achievement, the achievement updates the UI.
	// Four hops across four channels and four listeners, all of it inside the
	// drain that delivered the first event.
	void DispatchCycleTest::cascade()
	{
		subtest("AcrossChannels", [&]() { cascadeAcrossChannels(); });
		subtest("SelfReferential", [&]() { cascadeSelfReferential(); });
		subtest("ThroughSystems", [&]() { cascadeThroughSystems(); });
	}

	void DispatchCycleTest::cascadeAcrossChannels()
	{
		EventHarness harness(flavor(), "cascade-dispatcher");
		const cge::event::EventChannel<int> &damage = harness.registry.getChannel<int>("casc-damage");
		const cge::event::EventChannel<int> &death = harness.registry.getChannel<int>("casc-death");
		const cge::event::EventChannel<int> &score = harness.registry.getChannel<int>("casc-score");
		const cge::event::EventChannel<int> &achievement = harness.registry.getChannel<int>("casc-achievement");
		const cge::event::EventChannel<int> &ui = harness.registry.getChannel<int>("casc-ui");

		std::vector<int> hops;
		int delivered = 0;

		cge::event::BroadcasterBase broadcaster(&harness.dispatcher());
		cge::event::ListenerBase onDamage(&harness.dispatcher());
		cge::event::ListenerBase onDeath(&harness.dispatcher());
		cge::event::ListenerBase onScore(&harness.dispatcher());
		cge::event::ListenerBase onAchievement(&harness.dispatcher());
		cge::event::ListenerBase onUi(&harness.dispatcher());

		onDamage.requestRegister(damage, [&](const int &v) {
			hops.push_back(1);
			broadcaster.broadcast(death, v);
		});
		onDeath.requestRegister(death, [&](const int &v) {
			hops.push_back(2);
			broadcaster.broadcast(score, v);
		});
		onScore.requestRegister(score, [&](const int &v) {
			hops.push_back(3);
			broadcaster.broadcast(achievement, v);
		});
		onAchievement.requestRegister(achievement, [&](const int &v) {
			hops.push_back(4);
			broadcaster.broadcast(ui, v);
		});
		onUi.requestRegister(ui, [&](const int &v) {
			hops.push_back(5);
			delivered = v;
		});
		harness.dispatcher().dispatchCommands();

		broadcaster.broadcast(damage, 7);
		harness.dispatcher().dispatchEvents();

		// One drain, five hops, each exactly once and in order.
		ASSERT_EQUAL(hops.size(), static_cast<size_t>(5));
		if(hops.size() == 5)
		{
			for(size_t hop = 0; hop < hops.size(); ++hop)
				ASSERT_EQUAL(hops[hop], static_cast<int>(hop) + 1);
		}

		// The payload is carried the whole way rather than regenerated.
		ASSERT_EQUAL(delivered, 7);
	}

	// Reentry drains to exhaustion, so a handler that rebroadcasts on the very
	// channel it is being drained from feeds that same drain, to whatever depth
	// the producer chooses. The bound sits in the handler because the dispatcher
	// deliberately has none and never will: a cascade that does not stop is a
	// program that does not stop, which is the producer's defect to fix.
	//
	// ReentryBroadcasts covers this at depth one. What is added here is that the
	// depth is unbounded in practice, not that a single hop works.
	void DispatchCycleTest::cascadeSelfReferential()
	{
		EventHarness harness(flavor(), "cascade-self-dispatcher");
		const cge::event::EventChannel<int> &channel = harness.registry.getChannel<int>("casc-self");
		const int depth = 64;

		CountingListener listener(&harness.dispatcher());
		cge::event::BroadcasterBase broadcaster(&harness.dispatcher());

		listener.requestRegister(channel, [&](const int &v) {
			listener.onInt(v);
			if(v < depth)
				broadcaster.broadcast(channel, v + 1);
		});
		harness.dispatcher().dispatchCommands();

		broadcaster.broadcast(channel, 1);
		harness.dispatcher().dispatchEvents();

		// One drain, sixty-four hops, in order and none skipped.
		ASSERT_EQUAL(listener.received.size(), static_cast<size_t>(depth));
		if(listener.received.size() == static_cast<size_t>(depth))
		{
			for(int step = 0; step < depth; ++step)
				ASSERT_EQUAL(listener.received[step], step + 1);
		}
	}

	// Same chain, but each node is one object that both hears and emits, which
	// is how a real system is built. Nothing here should differ from the version
	// using free-standing listeners and broadcasters; the case exists because
	// nothing covered a single object sitting on both sides of a drain.
	void DispatchCycleTest::cascadeThroughSystems()
	{
		EventHarness harness(flavor(), "cascade-systems-dispatcher");
		const cge::event::EventChannel<int> &damage = harness.registry.getChannel<int>("sys-damage");
		const cge::event::EventChannel<int> &death = harness.registry.getChannel<int>("sys-death");
		const cge::event::EventChannel<int> &score = harness.registry.getChannel<int>("sys-score");
		const cge::event::EventChannel<int> &ui = harness.registry.getChannel<int>("sys-ui");

		std::vector<int> hops;
		int delivered = 0;

		RelayNode combat(&harness.dispatcher());
		RelayNode scoring(&harness.dispatcher());
		RelayNode achievements(&harness.dispatcher());
		cge::event::ListenerBase display(&harness.dispatcher());

		combat.listener.requestRegister(damage, [&](const int &v) {
			hops.push_back(1);
			combat.broadcaster.broadcast(death, v);
		});
		scoring.listener.requestRegister(death, [&](const int &v) {
			hops.push_back(2);
			scoring.broadcaster.broadcast(score, v);
		});
		achievements.listener.requestRegister(score, [&](const int &v) {
			hops.push_back(3);
			achievements.broadcaster.broadcast(ui, v);
		});

		// The last node only listens. Not every system needs to emit.
		display.requestRegister(ui, [&](const int &v) {
			hops.push_back(4);
			delivered = v;
		});
		harness.dispatcher().dispatchCommands();

		combat.broadcaster.broadcast(damage, 11);
		harness.dispatcher().dispatchEvents();

		ASSERT_EQUAL(hops.size(), static_cast<size_t>(4));
		if(hops.size() == 4)
		{
			for(size_t hop = 0; hop < hops.size(); ++hop)
				ASSERT_EQUAL(hops[hop], static_cast<int>(hop) + 1);
		}

		ASSERT_EQUAL(delivered, 11);
	}

	// The invariant is that the listener list for a draining channel is never
	// restructured before that drain finishes. Existing coverage only has a
	// listener mutate itself; here one listener mutates another, which is the
	// case a swap-and-pop removal could actually corrupt.
	//
	// Delivery order among listeners is not a contract, so nothing below depends
	// on which listener the handler runs against first. Five listeners is enough
	// that a removal relocates one, so a mutation leaking into the live list
	// would show up as a skipped or doubled delivery.
	void DispatchCycleTest::midDrainMutation()
	{
		subtest("UnregisterAnother", [&]() {
			EventHarness harness(flavor(), "mutate-unreg-dispatcher");
			const cge::event::EventChannel<int> &channel = harness.registry.getChannel<int>("mutate-unreg");
			cge::event::BroadcasterBase broadcaster(&harness.dispatcher());

			CountingListener a(&harness.dispatcher());
			CountingListener b(&harness.dispatcher());
			CountingListener c(&harness.dispatcher());
			CountingListener d(&harness.dispatcher());
			CountingListener e(&harness.dispatcher());

			a.requestRegister(channel, [&](const int &v) {
				a.onInt(v);
				d.requestUnregister(channel);
			});
			b.requestRegister(channel, [&b](const int &v) { b.onInt(v); });
			c.requestRegister(channel, [&c](const int &v) { c.onInt(v); });
			d.requestRegister(channel, [&d](const int &v) { d.onInt(v); });
			e.requestRegister(channel, [&e](const int &v) { e.onInt(v); });
			harness.dispatcher().dispatchCommands();

			broadcaster.broadcast(channel, 1);
			harness.dispatcher().dispatchEvents();

			// Deferral means d is still registered for the whole of this drain,
			// whether the handler on a ran before or after it.
			ASSERT_EQUAL(a.received.size(), static_cast<size_t>(1));
			ASSERT_EQUAL(b.received.size(), static_cast<size_t>(1));
			ASSERT_EQUAL(c.received.size(), static_cast<size_t>(1));
			ASSERT_EQUAL(d.received.size(), static_cast<size_t>(1));
			ASSERT_EQUAL(e.received.size(), static_cast<size_t>(1));

			harness.dispatcher().dispatchCommands();
			broadcaster.broadcast(channel, 2);
			harness.dispatcher().dispatchEvents();

			// The removal lands now, and the survivors are all still reachable.
			ASSERT_EQUAL(a.received.size(), static_cast<size_t>(2));
			ASSERT_EQUAL(b.received.size(), static_cast<size_t>(2));
			ASSERT_EQUAL(c.received.size(), static_cast<size_t>(2));
			ASSERT_EQUAL(d.received.size(), static_cast<size_t>(1));
			ASSERT_EQUAL(e.received.size(), static_cast<size_t>(2));
		});

		subtest("RegisterAnother", [&]() {
			EventHarness harness(flavor(), "mutate-reg-dispatcher");
			const cge::event::EventChannel<int> &channel = harness.registry.getChannel<int>("mutate-reg");
			cge::event::BroadcasterBase broadcaster(&harness.dispatcher());

			CountingListener a(&harness.dispatcher());
			CountingListener b(&harness.dispatcher());
			CountingListener c(&harness.dispatcher());
			CountingListener d(&harness.dispatcher());
			CountingListener newcomer(&harness.dispatcher());
			bool requested = false;

			a.requestRegister(channel, [&](const int &v) {
				a.onInt(v);
				if(requested)
					return;

				requested = true;
				newcomer.requestRegister(channel, [&newcomer](const int &value) {
					newcomer.onInt(value);
				});
			});
			b.requestRegister(channel, [&b](const int &v) { b.onInt(v); });
			c.requestRegister(channel, [&c](const int &v) { c.onInt(v); });
			d.requestRegister(channel, [&d](const int &v) { d.onInt(v); });
			harness.dispatcher().dispatchCommands();

			broadcaster.broadcast(channel, 1);
			harness.dispatcher().dispatchEvents();

			// The list being drained is not extended underneath the drain.
			ASSERT_EQUAL(newcomer.received.size(), static_cast<size_t>(0));
			ASSERT_EQUAL(a.received.size(), static_cast<size_t>(1));
			ASSERT_EQUAL(b.received.size(), static_cast<size_t>(1));
			ASSERT_EQUAL(c.received.size(), static_cast<size_t>(1));
			ASSERT_EQUAL(d.received.size(), static_cast<size_t>(1));

			harness.dispatcher().dispatchCommands();
			broadcaster.broadcast(channel, 2);
			harness.dispatcher().dispatchEvents();

			ASSERT_EQUAL(newcomer.received.size(), static_cast<size_t>(1));
			if(newcomer.received.size() == 1)
				ASSERT_EQUAL(newcomer.received[0], 2);
		});
	}
}
