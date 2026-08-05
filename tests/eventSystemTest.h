#ifndef EVENT_SYSTEM_TEST_H
#define EVENT_SYSTEM_TEST_H

#include <algorithm>
#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <semaphore>
#include <string>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

#include <partest/testbase.h>

#include "asyncDispatcher.h"
#include "broadcaster.h"
#include "event.h"
#include "listener.h"

namespace
{
	// Registry first so it outlives the dispatcher (reverse destruction order).
	struct AsyncEventHarness
	{
		cge::event::EventChannelRegistry registry;
		cge::event::AsyncDispatcher dispatcher;

		AsyncEventHarness()
			: registry()
			, dispatcher("AsyncEventTestDispatcher", &registry)
		{
			dispatcher.setUp();
		}

		~AsyncEventHarness()
		{
			dispatcher.tearDown();
		}
	};

	struct CountingListener : public cge::event::ListenerBase
	{
		std::vector<int> received;

		explicit CountingListener(cge::event::DispatcherBase *dispatcher)
			: ListenerBase(dispatcher)
		{
		}

		void onInt(const int &value)
		{
			received.push_back(value);
		}
	};
}

// ---------------------------------------------------------------------------
// EventChannelRegistry + channel construction policy
// ---------------------------------------------------------------------------
class EventChannelRegistryTest : public partest::TestBase
{
public:
	EventChannelRegistryTest()
		: TestBase("EventChannelRegistryTest", "Channel registry creation and construction policy.")
	{
		partest::TestFlags flags = partest::TEST_FLAGS_INHERIT;

		addTest("GetChannelCreatesAndReusesSameInstance", flags, [this]() { getChannelCreatesAndReusesSameInstance(); });
		addTest("GetChannelTypeMismatchThrows", flags, [this]() { getChannelTypeMismatchThrows(); });
		addTest("ChannelNotDefaultConstructibleOutsideRegistry", flags, [this]() { channelNotDefaultConstructibleOutsideRegistry(); });
		addTest("ChannelIsCopyConstructible", flags, [this]() { channelIsCopyConstructible(); });
		addTest("ChannelIsNotMoveConstructible", flags, [this]() { channelIsNotMoveConstructible(); });
		addTest("DistinctNamesGetDistinctIds", flags, [this]() { distinctNamesGetDistinctIds(); });
		addTest("MoveConstructionAndAssignmentPreserveChannels", flags, [this]() { moveConstructionAndAssignmentPreserveChannels(); });
	}

	void getChannelCreatesAndReusesSameInstance()
	{
		cge::event::EventChannelRegistry registry;
		const cge::event::EventChannel<int> &first = registry.getChannel<int>("alpha");
		const cge::event::EventChannel<int> &second = registry.getChannel<int>("alpha");

		ASSERT_EQUAL(first.id(), second.id());
		ASSERT_TRUE(&first == &second);
	}

	void getChannelTypeMismatchThrows()
	{
		cge::event::EventChannelRegistry registry;
		registry.getChannel<int>("typed");

		ASSERT_THROWS(std::runtime_error, registry.getChannel<std::string>("typed"));
	}

	void channelNotDefaultConstructibleOutsideRegistry()
	{
		// Protected default ctor: new channels must come from the registry.
		ASSERT_FALSE(std::is_default_constructible<cge::event::EventChannel<int>>::value);
		ASSERT_FALSE(std::is_default_constructible<cge::event::EventChannelBase>::value);
	}

	void channelIsCopyConstructible()
	{
		ASSERT_TRUE(std::is_copy_constructible<cge::event::EventChannel<int>>::value);

		cge::event::EventChannelRegistry registry;
		const cge::event::EventChannel<int> &original = registry.getChannel<int>("copyable");
		cge::event::EventChannel<int> copy(original);

		ASSERT_EQUAL(copy.id(), original.id());
	}

	void channelIsNotMoveConstructible()
	{
		ASSERT_FALSE(std::is_move_constructible<cge::event::EventChannel<int>>::value);
		ASSERT_FALSE(std::is_move_constructible<cge::event::EventChannelBase>::value);
	}

	void distinctNamesGetDistinctIds()
	{
		cge::event::EventChannelRegistry registry;
		const cge::event::EventChannel<int> &a = registry.getChannel<int>("a");
		const cge::event::EventChannel<int> &b = registry.getChannel<int>("b");

		ASSERT_NOT_EQUAL(a.id(), b.id());
	}

	// Channels are heap-owned, so identities must survive registry moves; move
	// assignment destroys the target's own channels first.
	void moveConstructionAndAssignmentPreserveChannels()
	{
		cge::event::EventChannelRegistry source;
		const cge::event::ChannelId id = source.getChannel<int>("mv").id();

		cge::event::EventChannelRegistry movedTo(std::move(source));
		ASSERT_EQUAL(movedTo.getChannel<int>("mv").id(), id);

		cge::event::EventChannelRegistry assigned;
		assigned.getChannel<int>("mv-old");
		assigned = std::move(movedTo);
		ASSERT_EQUAL(assigned.getChannel<int>("mv").id(), id);
	}
};

// ---------------------------------------------------------------------------
// AsyncDispatcher
// ---------------------------------------------------------------------------
class AsyncDispatcherTest : public partest::TestBase
{
public:
	AsyncDispatcherTest()
		: TestBase("AsyncDispatcherTest", "Async dispatcher push, drain, and lifecycle.")
	{
		partest::TestFlags flags = partest::TEST_FLAGS_INHERIT;

		addTest("DispatchOnEmptyQueuesIsNoOp", flags, [this]() { dispatchOnEmptyQueuesIsNoOp(); });
		addTest("RejectsPushBeforeSetUp", flags, [this]() { rejectsPushBeforeSetUp(); });
		addTest("RejectsPushAfterTearDown", flags, [this]() { rejectsPushAfterTearDown(); });
		addTest("EventsDoNotRunUntilDispatchEvents", flags, [this]() { eventsDoNotRunUntilDispatchEvents(); });
		addTest("RegistrationRequiresDispatchCommands", flags, [this]() { registrationRequiresDispatchCommands(); });
		addTest("DrainUntilEmptyHandlesReentryBroadcasts", flags, [this]() { drainUntilEmptyHandlesReentryBroadcasts(); });
		addTest("JoinedProducersAreDeliveredOnDispatch", flags, [this]() { joinedProducersAreDeliveredOnDispatch(); });
		addTest("UnknownCommandsAreConsumedWithoutListenerDelivery", flags, [this]() { unknownCommandsAreConsumedWithoutListenerDelivery(); });
		addTest("UnregisterMidDrainStillReceivesRestOfDrain", flags, [this]() { unregisterMidDrainStillReceivesRestOfDrain(); });
		addTest("QueuedEventsDrainAfterTearDown", flags, [this]() { queuedEventsDrainAfterTearDown(); });
		addTest("InactivePushIsDroppedNotDeferred", flags, [this]() { inactivePushIsDroppedNotDeferred(); });
		addTest("TwoDispatchersOnOneRegistryDoNotCrossTalk", flags, [this]() { twoDispatchersOnOneRegistryDoNotCrossTalk(); });
	}

	// Shared worker count for light concurrent smoke (not frame-scale load).
	static unsigned smokeProducerCount()
	{
		unsigned hc = std::thread::hardware_concurrency();
		if(hc < 2)
			return 2;
		if(hc > 4)
			return 4;
		return hc;
	}

	void dispatchOnEmptyQueuesIsNoOp()
	{
		AsyncEventHarness harness;
		ASSERT_NOTHROW(harness.dispatcher.dispatchCommands());
		ASSERT_NOTHROW(harness.dispatcher.dispatchEvents());
	}

	void rejectsPushBeforeSetUp()
	{
		cge::event::EventChannelRegistry registry;
		cge::event::AsyncDispatcher dispatcher("pre-setup", &registry);
		// deliberately not setUp()

		const cge::event::EventChannel<int> &channel = registry.getChannel<int>("pre");
		cge::event::ListenerBase listener(&dispatcher);
		cge::event::RegistrationResult result = listener.requestRegister(channel, [](const int &) {});

		ASSERT_TRUE(result == cge::event::RegistrationResult::Failure);
	}

	void rejectsPushAfterTearDown()
	{
		cge::event::EventChannelRegistry registry;
		cge::event::AsyncDispatcher dispatcher("post-teardown", &registry);
		dispatcher.setUp();
		dispatcher.tearDown();

		const cge::event::EventChannel<int> &channel = registry.getChannel<int>("post");
		cge::event::ListenerBase listener(&dispatcher);
		cge::event::RegistrationResult result = listener.requestRegister(channel, [](const int &) {});

		ASSERT_TRUE(result == cge::event::RegistrationResult::Failure);

		cge::event::BroadcasterBase broadcaster(&dispatcher);
		ASSERT_NOTHROW(broadcaster.broadcast(channel, 1));
	}

	void eventsDoNotRunUntilDispatchEvents()
	{
		AsyncEventHarness harness;
		const cge::event::EventChannel<int> &channel = harness.registry.getChannel<int>("deferred");
		CountingListener listener(&harness.dispatcher);

		ASSERT_TRUE(listener.requestRegister(channel, [&listener](const int &v) { listener.onInt(v); })
			== cge::event::RegistrationResult::Pending);
		harness.dispatcher.dispatchCommands();

		cge::event::BroadcasterBase broadcaster(&harness.dispatcher);
		broadcaster.broadcast(channel, 42);

		ASSERT_EQUAL(listener.received.size(), static_cast<size_t>(0));

		harness.dispatcher.dispatchEvents();

		ASSERT_EQUAL(listener.received.size(), static_cast<size_t>(1));
		ASSERT_EQUAL(listener.received[0], 42);
	}

	void registrationRequiresDispatchCommands()
	{
		AsyncEventHarness harness;
		const cge::event::EventChannel<int> &channel = harness.registry.getChannel<int>("needs-cmd");
		CountingListener listener(&harness.dispatcher);

		listener.requestRegister(channel, [&listener](const int &v) { listener.onInt(v); });

		cge::event::BroadcasterBase broadcaster(&harness.dispatcher);
		broadcaster.broadcast(channel, 7);
		// Registration still pending; listener map not updated yet.
		harness.dispatcher.dispatchEvents();
		ASSERT_EQUAL(listener.received.size(), static_cast<size_t>(0));

		harness.dispatcher.dispatchCommands();
		broadcaster.broadcast(channel, 8);
		harness.dispatcher.dispatchEvents();

		ASSERT_EQUAL(listener.received.size(), static_cast<size_t>(1));
		ASSERT_EQUAL(listener.received[0], 8);
	}

	void drainUntilEmptyHandlesReentryBroadcasts()
	{
		AsyncEventHarness harness;
		const cge::event::EventChannel<int> &channel = harness.registry.getChannel<int>("reentry");
		CountingListener listener(&harness.dispatcher);
		cge::event::BroadcasterBase broadcaster(&harness.dispatcher);

		listener.requestRegister(channel, [&](const int &v) {
			listener.onInt(v);
			if(v == 1)
				broadcaster.broadcast(channel, 2);
		});
		harness.dispatcher.dispatchCommands();

		broadcaster.broadcast(channel, 1);
		harness.dispatcher.dispatchEvents();

		ASSERT_EQUAL(listener.received.size(), static_cast<size_t>(2));
		ASSERT_EQUAL(listener.received[0], 1);
		ASSERT_EQUAL(listener.received[1], 2);
	}

	// Smoke test: producers join before the drain, so this never overlaps push
	// with dispatch. Push-during-drain coverage lives in the continuous load tests.
	void joinedProducersAreDeliveredOnDispatch()
	{
		AsyncEventHarness harness;
		const cge::event::EventChannel<int> &channel = harness.registry.getChannel<int>("mt");
		std::atomic<int> total(0);

		cge::event::ListenerBase listener(&harness.dispatcher);
		listener.requestRegister(channel, [&total](const int &v) { total.fetch_add(v); });
		harness.dispatcher.dispatchCommands();

		cge::event::BroadcasterBase broadcaster(&harness.dispatcher);
		const unsigned producers = smokeProducerCount();
		const int perProducer = 50;

		std::vector<std::thread> threads;
		for(unsigned p = 0; p < producers; ++p)
		{
			threads.emplace_back([&broadcaster, &channel, perProducer]() {
				for(int i = 0; i < perProducer; ++i)
					broadcaster.broadcast(channel, 1);
			});
		}
		for(std::thread &t : threads)
			t.join();

		harness.dispatcher.dispatchEvents();

		ASSERT_EQUAL(total.load(), static_cast<int>(producers) * perProducer);
	}

	// Commander/user commands are drained by dispatchCommands but are not fanned out
	// to listeners (only registration/unregistration channels are handled).
	void unknownCommandsAreConsumedWithoutListenerDelivery()
	{
		AsyncEventHarness harness;
		const cge::event::EventChannel<int> &channel = harness.registry.getChannel<int>("cmd-only");
		CountingListener listener(&harness.dispatcher);

		listener.requestRegister(channel, [&listener](const int &v) { listener.onInt(v); });
		harness.dispatcher.dispatchCommands();

		cge::event::CommanderBase commander(&harness.dispatcher);
		commander.command(channel, 99);
		harness.dispatcher.dispatchCommands();
		harness.dispatcher.dispatchEvents();

		ASSERT_EQUAL(listener.received.size(), static_cast<size_t>(0));
	}

	// Pending semantics: an unregistration requested mid-drain takes effect at the
	// next command drain; the rest of the current drain still delivers.
	void unregisterMidDrainStillReceivesRestOfDrain()
	{
		AsyncEventHarness harness;
		const cge::event::EventChannel<int> &channel = harness.registry.getChannel<int>("same-drain");
		CountingListener listener(&harness.dispatcher);

		listener.requestRegister(channel, [&](const int &v) {
			listener.onInt(v);
			if(v == 1)
				listener.requestUnregister(channel);
		});
		harness.dispatcher.dispatchCommands();

		cge::event::BroadcasterBase broadcaster(&harness.dispatcher);
		broadcaster.broadcast(channel, 1);
		broadcaster.broadcast(channel, 2);
		broadcaster.broadcast(channel, 3);
		harness.dispatcher.dispatchEvents();

		ASSERT_EQUAL(listener.received.size(), static_cast<size_t>(3));

		harness.dispatcher.dispatchCommands();
		broadcaster.broadcast(channel, 4);
		harness.dispatcher.dispatchEvents();

		ASSERT_EQUAL(listener.received.size(), static_cast<size_t>(3));
	}

	// tearDown stops intake, not processing: the engine keeps driving dispatch on
	// its schedule, and events queued while active still drain after teardown.
	// Early returns in dispatch are illegal by design.
	void queuedEventsDrainAfterTearDown()
	{
		cge::event::EventChannelRegistry registry;
		cge::event::AsyncDispatcher dispatcher("drain-after-teardown", &registry);
		dispatcher.setUp();

		const cge::event::EventChannel<int> &channel = registry.getChannel<int>("dat");
		CountingListener listener(&dispatcher);
		listener.requestRegister(channel, [&listener](const int &v) { listener.onInt(v); });
		dispatcher.dispatchCommands();

		cge::event::BroadcasterBase broadcaster(&dispatcher);
		broadcaster.broadcast(channel, 5);
		dispatcher.tearDown();

		dispatcher.dispatchEvents();
		ASSERT_EQUAL(listener.received.size(), static_cast<size_t>(1));
		ASSERT_EQUAL(listener.received[0], 5);
	}

	// Pushes while inactive are discarded, not parked for later. Dispatch always
	// drains, so a queued-but-unseen event would surface on the next drain —
	// nothing arriving is proof the push was refused outright.
	void inactivePushIsDroppedNotDeferred()
	{
		cge::event::EventChannelRegistry registry;
		cge::event::AsyncDispatcher dispatcher("inactive-drop", &registry);
		const cge::event::EventChannel<int> &channel = registry.getChannel<int>("drop");
		CountingListener listener(&dispatcher);
		cge::event::BroadcasterBase broadcaster(&dispatcher);

		// Before setUp.
		broadcaster.broadcast(channel, 1);

		dispatcher.setUp();
		listener.requestRegister(channel, [&listener](const int &v) { listener.onInt(v); });
		dispatcher.dispatchCommands();
		dispatcher.dispatchEvents();
		ASSERT_EQUAL(listener.received.size(), static_cast<size_t>(0));

		// After tearDown.
		dispatcher.tearDown();
		broadcaster.broadcast(channel, 2);
		dispatcher.dispatchEvents();
		ASSERT_EQUAL(listener.received.size(), static_cast<size_t>(0));
	}

	// Two dispatchers share one registry (and its named registration channels);
	// queues are per-dispatcher, so traffic must not cross.
	void twoDispatchersOnOneRegistryDoNotCrossTalk()
	{
		cge::event::EventChannelRegistry registry;
		cge::event::AsyncDispatcher first("dispatcher-a", &registry);
		cge::event::AsyncDispatcher second("dispatcher-b", &registry);
		first.setUp();
		second.setUp();

		const cge::event::EventChannel<int> &channel = registry.getChannel<int>("shared");
		CountingListener firstListener(&first);
		CountingListener secondListener(&second);

		firstListener.requestRegister(channel, [&firstListener](const int &v) { firstListener.onInt(v); });
		secondListener.requestRegister(channel, [&secondListener](const int &v) { secondListener.onInt(v); });
		first.dispatchCommands();
		second.dispatchCommands();

		cge::event::BroadcasterBase firstBroadcaster(&first);
		cge::event::BroadcasterBase secondBroadcaster(&second);
		firstBroadcaster.broadcast(channel, 1);
		secondBroadcaster.broadcast(channel, 2);
		first.dispatchEvents();
		second.dispatchEvents();

		ASSERT_EQUAL(firstListener.received.size(), static_cast<size_t>(1));
		ASSERT_EQUAL(firstListener.received[0], 1);
		ASSERT_EQUAL(secondListener.received.size(), static_cast<size_t>(1));
		ASSERT_EQUAL(secondListener.received[0], 2);

		first.tearDown();
		second.tearDown();
	}
};

// ---------------------------------------------------------------------------
// Game-shaped load: multi-frame worker traffic + payload integrity
//
// Mirrors Partest's dispatcher tests: keep a reference copy of every payload
// sent, collect everything received, then compare as a multiset (sort + walk).
// Cross-worker total order is not required.
//
// Workers are persistent. Frame-gated mode uses semaphores so production only
// happens between main-thread drains (no per-frame thread create/join).
// ---------------------------------------------------------------------------
class EventSystemLoadTest : public partest::TestBase
{
public:
	EventSystemLoadTest()
		: TestBase("EventSystemLoadTest", "Multi-frame loads with payload preservation checks.")
	{
		partest::TestFlags flags = partest::TEST_FLAGS_INHERIT;

		addTest("FrameGatedWorkersManyFrames", flags, [this]() { frameGatedWorkersManyFrames(); });
		addTest("ContinuousWorkersManyFrames", flags, [this]() { continuousWorkersManyFrames(); });
		addTest("FrameGatedCascadeManyFrames", flags, [this]() { frameGatedCascadeManyFrames(); });
		addTest("ContinuousCascadeManyFrames", flags, [this]() { continuousCascadeManyFrames(); });
		addTest("FrameGatedMixedEventAndCommandManyFrames", flags, [this]() { frameGatedMixedEventAndCommandManyFrames(); });
		addTest("HandlerRegistersAnotherChannelDuringFlush", flags, [this]() { handlerRegistersAnotherChannelDuringFlush(); });
		addTest("ConcurrentRegistrationChurnPreservesStableDelivery", flags, [this]() { concurrentRegistrationChurnPreservesStableDelivery(); });
	}

	static unsigned loadWorkerCount()
	{
		unsigned hc = std::thread::hardware_concurrency();
		if(hc < 2)
			return 2;
		if(hc > 8)
			return 8;
		return hc;
	}

	static const unsigned kPushesPerWorkerPerFrame = 512;
	static const unsigned kFrameCount = 32;

	// Thread-safe reference / receive log (same role as Partest's m_logs + reporter logs).
	struct PayloadLog
	{
		std::mutex mutex;
		std::vector<int> values;

		void record(int value)
		{
			std::lock_guard<std::mutex> lock(mutex);
			values.push_back(value);
		}

		std::vector<int> snapshot()
		{
			std::lock_guard<std::mutex> lock(mutex);
			return values;
		}
	};

	// Unique payload: frame, worker, and sequence are all recoverable if a test fails.
	static int makePayload(unsigned frame, unsigned worker, unsigned seq)
	{
		// Generous packing for the volumes used here (frames * workers * seq fits in 32-bit).
		return static_cast<int>((frame << 20) | (worker << 12) | seq);
	}

	// Sort copies and require identical multisets (order of arrival ignored).
	// Asserts record and continue, so the scan must stay in bounds whether or not
	// the size check passed; on divergence, report the first differing pair.
	void assertPayloadsPreserved(const std::vector<int> &sent, const std::vector<int> &received)
	{
		ASSERT_EQUAL(received.size(), sent.size());

		std::vector<int> expected = sent;
		std::vector<int> actual = received;
		std::sort(expected.begin(), expected.end());
		std::sort(actual.begin(), actual.end());

		const size_t bound = std::min(expected.size(), actual.size());
		for(size_t i = 0; i < bound; ++i)
		{
			if(expected[i] != actual[i])
			{
				ASSERT_EQUAL(actual[i], expected[i]);
				return;
			}
		}
	}

	// --- Frame-gated: persistent workers, semaphore between produce and drain -----------

	void frameGatedWorkersManyFrames()
	{
		AsyncEventHarness harness;
		const cge::event::EventChannel<int> &channel = harness.registry.getChannel<int>("load-gated");
		PayloadLog sent;
		PayloadLog received;

		cge::event::ListenerBase listener(&harness.dispatcher);
		listener.requestRegister(channel, [&received](const int &v) { received.record(v); });
		harness.dispatcher.dispatchCommands();

		cge::event::BroadcasterBase broadcaster(&harness.dispatcher);
		const unsigned workers = loadWorkerCount();

		const bool completed = runPersistentFrameGated(workers, kFrameCount, kPushesPerWorkerPerFrame,
			[&](unsigned frame, unsigned worker, unsigned seq) {
				const int payload = makePayload(frame, worker, seq);
				sent.record(payload);
				broadcaster.broadcast(channel, payload);
			},
			[&]() {
				harness.dispatcher.dispatchEvents();
			});
		ASSERT_TRUE(completed);

		assertPayloadsPreserved(sent.snapshot(), received.snapshot());
	}

	void frameGatedCascadeManyFrames()
	{
		AsyncEventHarness harness;
		const cge::event::EventChannel<int> &primary = harness.registry.getChannel<int>("load-gated-casc-a");
		const cge::event::EventChannel<int> &secondary = harness.registry.getChannel<int>("load-gated-casc-b");
		PayloadLog sent;
		PayloadLog receivedPrimary;
		PayloadLog receivedSecondary;

		cge::event::BroadcasterBase broadcaster(&harness.dispatcher);
		cge::event::ListenerBase primaryListener(&harness.dispatcher);
		cge::event::ListenerBase secondaryListener(&harness.dispatcher);

		primaryListener.requestRegister(primary, [&](const int &v) {
			receivedPrimary.record(v);
			// Cascade preserves the same payload identity on the secondary channel.
			broadcaster.broadcast(secondary, v);
		});
		secondaryListener.requestRegister(secondary, [&](const int &v) {
			receivedSecondary.record(v);
		});
		harness.dispatcher.dispatchCommands();

		const unsigned workers = loadWorkerCount();
		const unsigned perWorker = kPushesPerWorkerPerFrame / 2;

		const bool completed = runPersistentFrameGated(workers, kFrameCount, perWorker,
			[&](unsigned frame, unsigned worker, unsigned seq) {
				const int payload = makePayload(frame, worker, seq);
				sent.record(payload);
				broadcaster.broadcast(primary, payload);
			},
			[&]() {
				harness.dispatcher.dispatchEvents();
			});
		ASSERT_TRUE(completed);

		std::vector<int> expected = sent.snapshot();
		assertPayloadsPreserved(expected, receivedPrimary.snapshot());
		assertPayloadsPreserved(expected, receivedSecondary.snapshot());
	}

	void frameGatedMixedEventAndCommandManyFrames()
	{
		AsyncEventHarness harness;
		const cge::event::EventChannel<int> &channel = harness.registry.getChannel<int>("load-gated-mixed");
		PayloadLog sent;
		PayloadLog received;

		cge::event::ListenerBase listener(&harness.dispatcher);
		listener.requestRegister(channel, [&received](const int &v) { received.record(v); });
		harness.dispatcher.dispatchCommands();

		cge::event::BroadcasterBase broadcaster(&harness.dispatcher);
		cge::event::CommanderBase commander(&harness.dispatcher);
		const unsigned workers = loadWorkerCount();
		const unsigned commandPushes = 64;

		const bool completed = runPersistentFrameGated(workers, kFrameCount, kPushesPerWorkerPerFrame,
			[&](unsigned frame, unsigned worker, unsigned seq) {
				const int payload = makePayload(frame, worker, seq);
				sent.record(payload);
				broadcaster.broadcast(channel, payload);
				// Command noise must not steal or corrupt event payloads.
				if(seq < commandPushes)
					commander.command(channel, -1);
			},
			[&]() {
				harness.dispatcher.dispatchCommands();
				harness.dispatcher.dispatchEvents();
			});
		ASSERT_TRUE(completed);

		assertPayloadsPreserved(sent.snapshot(), received.snapshot());
	}

	// --- Continuous: persistent workers fire the whole run; main steps many frames -----

	void continuousWorkersManyFrames()
	{
		AsyncEventHarness harness;
		const cge::event::EventChannel<int> &channel = harness.registry.getChannel<int>("load-cont");
		PayloadLog sent;
		PayloadLog received;

		cge::event::ListenerBase listener(&harness.dispatcher);
		listener.requestRegister(channel, [&received](const int &v) { received.record(v); });
		harness.dispatcher.dispatchCommands();

		cge::event::BroadcasterBase broadcaster(&harness.dispatcher);
		const unsigned workers = loadWorkerCount();
		const unsigned pushesPerWorker = kPushesPerWorkerPerFrame * kFrameCount;

		std::vector<std::thread> threads;
		threads.reserve(workers);
		for(unsigned w = 0; w < workers; ++w)
		{
			threads.emplace_back([&broadcaster, &channel, &sent, w, pushesPerWorker]() {
				for(unsigned i = 0; i < pushesPerWorker; ++i)
				{
					// frame slot folded into high bits via i / perFrame for uniqueness.
					const unsigned frame = i / kPushesPerWorkerPerFrame;
					const unsigned seq = i % kPushesPerWorkerPerFrame;
					const int payload = makePayload(frame, w, seq);
					sent.record(payload);
					broadcaster.broadcast(channel, payload);
				}
			});
		}

		for(unsigned frame = 0; frame < kFrameCount; ++frame)
			harness.dispatcher.dispatchEvents();

		for(std::thread &t : threads)
			t.join();

		for(unsigned extra = 0; extra < 8; ++extra)
			harness.dispatcher.dispatchEvents();

		assertPayloadsPreserved(sent.snapshot(), received.snapshot());
	}

	void continuousCascadeManyFrames()
	{
		AsyncEventHarness harness;
		const cge::event::EventChannel<int> &primary = harness.registry.getChannel<int>("load-cont-casc-a");
		const cge::event::EventChannel<int> &secondary = harness.registry.getChannel<int>("load-cont-casc-b");
		PayloadLog sent;
		PayloadLog receivedPrimary;
		PayloadLog receivedSecondary;

		cge::event::BroadcasterBase broadcaster(&harness.dispatcher);
		cge::event::ListenerBase primaryListener(&harness.dispatcher);
		cge::event::ListenerBase secondaryListener(&harness.dispatcher);

		primaryListener.requestRegister(primary, [&](const int &v) {
			receivedPrimary.record(v);
			broadcaster.broadcast(secondary, v);
		});
		secondaryListener.requestRegister(secondary, [&](const int &v) {
			receivedSecondary.record(v);
		});
		harness.dispatcher.dispatchCommands();

		const unsigned workers = loadWorkerCount();
		const unsigned pushesPerWorker = (kPushesPerWorkerPerFrame / 2) * kFrameCount;
		const unsigned perFrame = kPushesPerWorkerPerFrame / 2;

		std::vector<std::thread> threads;
		for(unsigned w = 0; w < workers; ++w)
		{
			threads.emplace_back([&broadcaster, &primary, &sent, w, pushesPerWorker, perFrame]() {
				for(unsigned i = 0; i < pushesPerWorker; ++i)
				{
					const unsigned frame = i / perFrame;
					const unsigned seq = i % perFrame;
					const int payload = makePayload(frame, w, seq);
					sent.record(payload);
					broadcaster.broadcast(primary, payload);
				}
			});
		}

		for(unsigned frame = 0; frame < kFrameCount; ++frame)
			harness.dispatcher.dispatchEvents();

		for(std::thread &t : threads)
			t.join();

		for(unsigned extra = 0; extra < 8; ++extra)
			harness.dispatcher.dispatchEvents();

		std::vector<int> expected = sent.snapshot();
		assertPayloadsPreserved(expected, receivedPrimary.snapshot());
		assertPayloadsPreserved(expected, receivedSecondary.snapshot());
	}

	void handlerRegistersAnotherChannelDuringFlush()
	{
		AsyncEventHarness harness;
		const cge::event::EventChannel<int> &kick = harness.registry.getChannel<int>("load-reg-kick");
		const cge::event::EventChannel<int> &late = harness.registry.getChannel<int>("load-reg-late");
		std::atomic<int> lateHits(0);
		std::atomic<bool> requested(false);

		cge::event::BroadcasterBase broadcaster(&harness.dispatcher);
		cge::event::ListenerBase kickListener(&harness.dispatcher);
		cge::event::ListenerBase lateListener(&harness.dispatcher);

		kickListener.requestRegister(kick, [&](const int &) {
			if(!requested.exchange(true))
			{
				lateListener.requestRegister(late, [&lateHits](const int &) {
					lateHits.fetch_add(1);
				});
			}
		});
		harness.dispatcher.dispatchCommands();

		broadcaster.broadcast(kick, 1);
		harness.dispatcher.dispatchEvents();
		ASSERT_EQUAL(lateHits.load(), 0);

		harness.dispatcher.dispatchCommands();
		broadcaster.broadcast(late, 99);
		harness.dispatcher.dispatchEvents();

		ASSERT_EQUAL(lateHits.load(), 1);
	}

	// Registration churn from worker threads while producers broadcast — the
	// scenario the pending-handler mutex exists for. Only determinism-proof
	// invariants are asserted: the stable listener sees every payload, and nothing
	// crashes. What the churning listeners receive depends on drain timing and is
	// deliberately unasserted.
	void concurrentRegistrationChurnPreservesStableDelivery()
	{
		AsyncEventHarness harness;
		const cge::event::EventChannel<int> &channel = harness.registry.getChannel<int>("churn");
		PayloadLog sent;
		PayloadLog received;

		cge::event::ListenerBase stable(&harness.dispatcher);
		stable.requestRegister(channel, [&received](const int &v) { received.record(v); });
		harness.dispatcher.dispatchCommands();

		cge::event::BroadcasterBase broadcaster(&harness.dispatcher);
		const unsigned producers = 2;
		const unsigned churners = 2;
		const unsigned perProducer = kPushesPerWorkerPerFrame * 8;
		const unsigned churnCycles = 256;
		std::atomic<unsigned> runningWorkers(producers + churners);

		std::vector<std::unique_ptr<cge::event::ListenerBase>> churnListeners;
		for(unsigned c = 0; c < churners; ++c)
			churnListeners.push_back(std::make_unique<cge::event::ListenerBase>(&harness.dispatcher));

		std::vector<std::thread> threads;
		for(unsigned p = 0; p < producers; ++p)
		{
			threads.emplace_back([&, p]() {
				for(unsigned i = 0; i < perProducer; ++i)
				{
					const int payload = makePayload(i / kPushesPerWorkerPerFrame, p, i % kPushesPerWorkerPerFrame);
					sent.record(payload);
					broadcaster.broadcast(channel, payload);
				}
				runningWorkers.fetch_sub(1);
			});
		}
		for(unsigned c = 0; c < churners; ++c)
		{
			cge::event::ListenerBase *churner = churnListeners[c].get();
			threads.emplace_back([&, churner]() {
				for(unsigned i = 0; i < churnCycles; ++i)
				{
					// Results ignored: Duplicate/NotFound are legal under churn.
					churner->requestRegister(channel, [](const int &) {});
					std::this_thread::yield();
					churner->requestUnregister(channel);
					std::this_thread::yield();
				}
				runningWorkers.fetch_sub(1);
			});
		}

		while(runningWorkers.load() > 0)
		{
			harness.dispatcher.dispatchCommands();
			harness.dispatcher.dispatchEvents();
		}
		for(std::thread &t : threads)
			t.join();

		// Trailing drains: apply any residual commands, then flush remaining events.
		for(unsigned extra = 0; extra < 4; ++extra)
		{
			harness.dispatcher.dispatchCommands();
			harness.dispatcher.dispatchEvents();
		}

		assertPayloadsPreserved(sent.snapshot(), received.snapshot());
	}

private:
	// Persistent workers. Each frame: main releases all workers, waits for all to finish
	// producing, then runs onFrameComplete (dispatch). Same gate idea as a job fence.
	// Returns false on watchdog timeout (after stopping and joining the workers) so a
	// wedged run fails its test instead of hanging the suite; callers assert the result.
	template<typename ProduceFn, typename FrameCompleteFn>
	static bool runPersistentFrameGated(
		unsigned workers,
		unsigned frameCount,
		unsigned pushesPerWorkerPerFrame,
		ProduceFn produce,
		FrameCompleteFn onFrameComplete)
	{
		std::counting_semaphore<> beginFrame(0);
		std::counting_semaphore<> endFrame(0);
		std::atomic<unsigned> currentFrame(0);
		std::atomic<bool> stop(false);

		std::vector<std::thread> threads;
		threads.reserve(workers);
		for(unsigned w = 0; w < workers; ++w)
		{
			threads.emplace_back([&, w]() {
				while(true)
				{
					beginFrame.acquire();
					if(stop.load(std::memory_order_acquire))
						break;

					const unsigned frame = currentFrame.load(std::memory_order_acquire);
					for(unsigned seq = 0; seq < pushesPerWorkerPerFrame; ++seq)
						produce(frame, w, seq);

					endFrame.release();
				}
			});
		}

		bool completed = true;
		for(unsigned frame = 0; frame < frameCount && completed; ++frame)
		{
			currentFrame.store(frame, std::memory_order_release);
			for(unsigned w = 0; w < workers; ++w)
				beginFrame.release();
			for(unsigned w = 0; w < workers; ++w)
			{
				if(!endFrame.try_acquire_for(std::chrono::seconds(30)))
				{
					completed = false;
					break;
				}
			}

			if(completed)
				onFrameComplete();
		}

		stop.store(true, std::memory_order_release);
		for(unsigned w = 0; w < workers; ++w)
			beginFrame.release();
		for(std::thread &t : threads)
			t.join();
		return completed;
	}
};

// ---------------------------------------------------------------------------
// BroadcasterBase
// ---------------------------------------------------------------------------
class BroadcasterBaseTest : public partest::TestBase
{
public:
	BroadcasterBaseTest()
		: TestBase("BroadcasterBaseTest", "Public broadcaster payload delivery.")
	{
		partest::TestFlags flags = partest::TEST_FLAGS_INHERIT;

		addTest("BroadcastDeliversTypedPayload", flags, [this]() { broadcastDeliversTypedPayload(); });
		addTest("BroadcastToChannelWithNoListenersDoesNotThrow", flags, [this]() { broadcastToChannelWithNoListenersDoesNotThrow(); });
		addTest("MultipleBroadcastsPreserveOrder", flags, [this]() { multipleBroadcastsPreserveOrder(); });
		addTest("StringPayloadRoundTrips", flags, [this]() { stringPayloadRoundTrips(); });
		addTest("StructPayloadRoundTrips", flags, [this]() { structPayloadRoundTrips(); });
		addTest("PointerPayloadTransfersAddress", flags, [this]() { pointerPayloadTransfersAddress(); });
		addTest("EnumPayloadRoundTrips", flags, [this]() { enumPayloadRoundTrips(); });
		addTest("AggregateWithClassMembersRoundTrips", flags, [this]() { aggregateWithClassMembersRoundTrips(); });
	}

	void broadcastDeliversTypedPayload()
	{
		AsyncEventHarness harness;
		const cge::event::EventChannel<int> &channel = harness.registry.getChannel<int>("bc-int");
		CountingListener listener(&harness.dispatcher);

		listener.requestRegister(channel, [&listener](const int &v) { listener.onInt(v); });
		harness.dispatcher.dispatchCommands();

		cge::event::BroadcasterBase broadcaster(&harness.dispatcher);
		broadcaster.broadcast(channel, 123);
		harness.dispatcher.dispatchEvents();

		ASSERT_EQUAL(listener.received.size(), static_cast<size_t>(1));
		ASSERT_EQUAL(listener.received[0], 123);
	}

	void broadcastToChannelWithNoListenersDoesNotThrow()
	{
		AsyncEventHarness harness;
		const cge::event::EventChannel<int> &channel = harness.registry.getChannel<int>("bc-empty");
		cge::event::BroadcasterBase broadcaster(&harness.dispatcher);

		ASSERT_NOTHROW(broadcaster.broadcast(channel, 1));
		ASSERT_NOTHROW(harness.dispatcher.dispatchEvents());
	}

	void multipleBroadcastsPreserveOrder()
	{
		AsyncEventHarness harness;
		const cge::event::EventChannel<int> &channel = harness.registry.getChannel<int>("bc-order");
		CountingListener listener(&harness.dispatcher);

		listener.requestRegister(channel, [&listener](const int &v) { listener.onInt(v); });
		harness.dispatcher.dispatchCommands();

		cge::event::BroadcasterBase broadcaster(&harness.dispatcher);
		broadcaster.broadcast(channel, 1);
		broadcaster.broadcast(channel, 2);
		broadcaster.broadcast(channel, 3);
		harness.dispatcher.dispatchEvents();

		ASSERT_EQUAL(listener.received.size(), static_cast<size_t>(3));
		ASSERT_EQUAL(listener.received[0], 1);
		ASSERT_EQUAL(listener.received[1], 2);
		ASSERT_EQUAL(listener.received[2], 3);
	}

	// Trivially-copyable aggregate — the archetypal game payload class.
	void structPayloadRoundTrips()
	{
		struct DamagePayload
		{
			int amount;
			float multiplier;
			unsigned sourceId;
		};
		static_assert(std::is_trivially_copyable<DamagePayload>::value,
			"representative must actually belong to the trivially-copyable class");

		AsyncEventHarness harness;
		const cge::event::EventChannel<DamagePayload> &channel = harness.registry.getChannel<DamagePayload>("bc-struct");
		DamagePayload got;
		got.amount = 0;
		got.multiplier = 0.0f;
		got.sourceId = 0;

		cge::event::ListenerBase listener(&harness.dispatcher);
		listener.requestRegister(channel, [&got](const DamagePayload &p) { got = p; });
		harness.dispatcher.dispatchCommands();

		DamagePayload payload;
		payload.amount = 25;
		payload.multiplier = 1.5f;
		payload.sourceId = 7;

		cge::event::BroadcasterBase broadcaster(&harness.dispatcher);
		broadcaster.broadcast(channel, payload);
		harness.dispatcher.dispatchEvents();

		ASSERT_EQUAL(got.amount, 25);
		ASSERT_EQUAL(got.multiplier, 1.5f);
		ASSERT_EQUAL(got.sourceId, 7u);
	}

	// Pointer payloads copy the address, not the pointee: the handler must see
	// the same object the broadcaster pointed at.
	void pointerPayloadTransfersAddress()
	{
		AsyncEventHarness harness;
		const cge::event::EventChannel<int*> &channel = harness.registry.getChannel<int*>("bc-ptr");
		int target = 41;
		int *got = nullptr;

		cge::event::ListenerBase listener(&harness.dispatcher);
		listener.requestRegister(channel, [&got](int *const &p) { got = p; });
		harness.dispatcher.dispatchCommands();

		cge::event::BroadcasterBase broadcaster(&harness.dispatcher);
		broadcaster.broadcast(channel, &target);
		harness.dispatcher.dispatchEvents();

		ASSERT_TRUE(got == &target);
		*got = 42;
		ASSERT_EQUAL(target, 42);
	}

	void enumPayloadRoundTrips()
	{
		enum class GameState { Menu, Loading, Playing };

		AsyncEventHarness harness;
		const cge::event::EventChannel<GameState> &channel = harness.registry.getChannel<GameState>("bc-enum");
		GameState got = GameState::Menu;

		cge::event::ListenerBase listener(&harness.dispatcher);
		listener.requestRegister(channel, [&got](const GameState &s) { got = s; });
		harness.dispatcher.dispatchCommands();

		cge::event::BroadcasterBase broadcaster(&harness.dispatcher);
		broadcaster.broadcast(channel, GameState::Playing);
		harness.dispatcher.dispatchEvents();

		ASSERT_TRUE(got == GameState::Playing);
	}

	// Aggregate whose members own resources: copy is member-wise and non-trivial —
	// distinct from both the bit-copyable struct and the bare string cases.
	void aggregateWithClassMembersRoundTrips()
	{
		struct SpawnRequest
		{
			int unitType;
			std::string name;
			std::vector<int> inventory;
		};

		AsyncEventHarness harness;
		const cge::event::EventChannel<SpawnRequest> &channel = harness.registry.getChannel<SpawnRequest>("bc-agg");
		SpawnRequest got;
		got.unitType = 0;

		cge::event::ListenerBase listener(&harness.dispatcher);
		listener.requestRegister(channel, [&got](const SpawnRequest &r) { got = r; });
		harness.dispatcher.dispatchCommands();

		SpawnRequest request;
		request.unitType = 3;
		request.name = "archer";
		request.inventory.push_back(10);
		request.inventory.push_back(20);

		cge::event::BroadcasterBase broadcaster(&harness.dispatcher);
		broadcaster.broadcast(channel, request);
		harness.dispatcher.dispatchEvents();

		ASSERT_EQUAL(got.unitType, 3);
		ASSERT_EQUAL(got.name, std::string("archer"));
		ASSERT_EQUAL(got.inventory.size(), static_cast<size_t>(2));
		ASSERT_EQUAL(got.inventory[0], 10);
		ASSERT_EQUAL(got.inventory[1], 20);
	}

	// Renamed from "DoesNotSlice": slicing was never possible here — Event<T>
	// stores the payload by value. This pins a resource-owning payload round trip.
	void stringPayloadRoundTrips()
	{
		AsyncEventHarness harness;
		const cge::event::EventChannel<std::string> &channel = harness.registry.getChannel<std::string>("bc-str");
		std::string got;

		cge::event::ListenerBase listener(&harness.dispatcher);
		listener.requestRegister(channel, [&got](const std::string &s) { got = s; });
		harness.dispatcher.dispatchCommands();

		cge::event::BroadcasterBase broadcaster(&harness.dispatcher);
		broadcaster.broadcast(channel, std::string("hello-event"));
		harness.dispatcher.dispatchEvents();

		ASSERT_EQUAL(got, std::string("hello-event"));
	}
};

// ---------------------------------------------------------------------------
// CommanderBase
// ---------------------------------------------------------------------------
class CommanderBaseTest : public partest::TestBase
{
public:
	CommanderBaseTest()
		: TestBase("CommanderBaseTest", "Public commander enqueue behavior.")
	{
		partest::TestFlags flags = partest::TEST_FLAGS_INHERIT;

		addTest("CommandDoesNotDeliverToChannelListeners", flags, [this]() { commandDoesNotDeliverToChannelListeners(); });
		addTest("CommandAfterTearDownDoesNotThrow", flags, [this]() { commandAfterTearDownDoesNotThrow(); });
		addTest("CommandQueueDoesNotCrossIntoEventListeners", flags, [this]() { commandQueueDoesNotCrossIntoEventListeners(); });
	}

	// Current design: dispatchCommands only applies registration/unregistration.
	// Arbitrary CommanderBase payloads are dequeued and discarded.
	void commandDoesNotDeliverToChannelListeners()
	{
		AsyncEventHarness harness;
		const cge::event::EventChannel<int> &channel = harness.registry.getChannel<int>("cmd-int");
		CountingListener listener(&harness.dispatcher);

		listener.requestRegister(channel, [&listener](const int &v) { listener.onInt(v); });
		harness.dispatcher.dispatchCommands();

		cge::event::CommanderBase commander(&harness.dispatcher);
		commander.command(channel, 55);
		harness.dispatcher.dispatchCommands();
		harness.dispatcher.dispatchEvents();

		ASSERT_EQUAL(listener.received.size(), static_cast<size_t>(0));
	}

	void commandAfterTearDownDoesNotThrow()
	{
		cge::event::EventChannelRegistry registry;
		cge::event::AsyncDispatcher dispatcher("cmd-td", &registry);
		dispatcher.setUp();
		const cge::event::EventChannel<int> &channel = registry.getChannel<int>("cmd-td-ch");
		cge::event::CommanderBase commander(&dispatcher);
		dispatcher.tearDown();

		ASSERT_NOTHROW(commander.command(channel, 1));
	}

	void commandQueueDoesNotCrossIntoEventListeners()
	{
		AsyncEventHarness harness;
		const cge::event::EventChannel<int> &channel = harness.registry.getChannel<int>("cmd-vs-evt");
		CountingListener listener(&harness.dispatcher);

		listener.requestRegister(channel, [&listener](const int &v) { listener.onInt(v); });
		harness.dispatcher.dispatchCommands();

		cge::event::CommanderBase commander(&harness.dispatcher);
		cge::event::BroadcasterBase broadcaster(&harness.dispatcher);

		commander.command(channel, 1);
		broadcaster.broadcast(channel, 2);
		// Drain commands first (drops the command payload), then events.
		harness.dispatcher.dispatchCommands();
		harness.dispatcher.dispatchEvents();

		ASSERT_EQUAL(listener.received.size(), static_cast<size_t>(1));
		ASSERT_EQUAL(listener.received[0], 2);
	}
};

// ---------------------------------------------------------------------------
// ListenerBase
// ---------------------------------------------------------------------------
class ListenerBaseTest : public partest::TestBase
{
public:
	ListenerBaseTest()
		: TestBase("ListenerBaseTest", "Listener registration, handlers, and unregister.")
	{
		partest::TestFlags flags = partest::TEST_FLAGS_INHERIT;

		addTest("RequestRegisterReturnsPendingWhenActive", flags, [this]() { requestRegisterReturnsPendingWhenActive(); });
		addTest("DuplicatePendingRegisterReturnsDuplicate", flags, [this]() { duplicatePendingRegisterReturnsDuplicate(); });
		addTest("MemberFunctionCallbackFormWorks", flags, [this]() { memberFunctionCallbackFormWorks(); });
		addTest("MultipleListenersReceiveSameEvent", flags, [this]() { multipleListenersReceiveSameEvent(); });
		addTest("UnregisterStopsFurtherDelivery", flags, [this]() { unregisterStopsFurtherDelivery(); });
		addTest("UnregisterNotRegisteredStillPendingThenNotFound", flags, [this]() { unregisterNotRegisteredStillPendingThenNotFound(); });
		addTest("SecondRegisterAfterSuccessStillAcceptedAsPending", flags, [this]() { secondRegisterAfterSuccessStillAcceptedAsPending(); });
		addTest("UnregistrationBeatsQueuedEvents", flags, [this]() { unregistrationBeatsQueuedEvents(); });
		addTest("UnregisterOneOfSeveralListenersOthersStillReceive", flags, [this]() { unregisterOneOfSeveralListenersOthersStillReceive(); });
		addTest("ListenerOnTwoChannelsUnregisterOneKeepsOther", flags, [this]() { listenerOnTwoChannelsUnregisterOneKeepsOther(); });
		addTest("RegisterUnregisterRegisterInOneBatch", flags, [this]() { registerUnregisterRegisterInOneBatch(); });
	}

	void requestRegisterReturnsPendingWhenActive()
	{
		AsyncEventHarness harness;
		const cge::event::EventChannel<int> &channel = harness.registry.getChannel<int>("reg-pending");
		cge::event::ListenerBase listener(&harness.dispatcher);

		cge::event::RegistrationResult result = listener.requestRegister(channel, [](const int &) {});
		ASSERT_TRUE(result == cge::event::RegistrationResult::Pending);
	}

	void duplicatePendingRegisterReturnsDuplicate()
	{
		AsyncEventHarness harness;
		const cge::event::EventChannel<int> &channel = harness.registry.getChannel<int>("reg-dup");
		cge::event::ListenerBase listener(&harness.dispatcher);

		ASSERT_TRUE(listener.requestRegister(channel, [](const int &) {})
			== cge::event::RegistrationResult::Pending);
		ASSERT_TRUE(listener.requestRegister(channel, [](const int &) {})
			== cge::event::RegistrationResult::Duplicate);
	}

	void memberFunctionCallbackFormWorks()
	{
		AsyncEventHarness harness;
		const cge::event::EventChannel<int> &channel = harness.registry.getChannel<int>("reg-member");
		CountingListener listener(&harness.dispatcher);

		ASSERT_TRUE(listener.requestRegister(channel, &listener, &CountingListener::onInt)
			== cge::event::RegistrationResult::Pending);
		harness.dispatcher.dispatchCommands();

		cge::event::BroadcasterBase broadcaster(&harness.dispatcher);
		broadcaster.broadcast(channel, 77);
		harness.dispatcher.dispatchEvents();

		ASSERT_EQUAL(listener.received.size(), static_cast<size_t>(1));
		ASSERT_EQUAL(listener.received[0], 77);
	}

	void multipleListenersReceiveSameEvent()
	{
		AsyncEventHarness harness;
		const cge::event::EventChannel<int> &channel = harness.registry.getChannel<int>("reg-multi");
		CountingListener a(&harness.dispatcher);
		CountingListener b(&harness.dispatcher);

		a.requestRegister(channel, [&a](const int &v) { a.onInt(v); });
		b.requestRegister(channel, [&b](const int &v) { b.onInt(v); });
		harness.dispatcher.dispatchCommands();

		cge::event::BroadcasterBase broadcaster(&harness.dispatcher);
		broadcaster.broadcast(channel, 9);
		harness.dispatcher.dispatchEvents();

		ASSERT_EQUAL(a.received.size(), static_cast<size_t>(1));
		ASSERT_EQUAL(b.received.size(), static_cast<size_t>(1));
		ASSERT_EQUAL(a.received[0], 9);
		ASSERT_EQUAL(b.received[0], 9);
	}

	void unregisterStopsFurtherDelivery()
	{
		AsyncEventHarness harness;
		const cge::event::EventChannel<int> &channel = harness.registry.getChannel<int>("reg-unreg");
		CountingListener listener(&harness.dispatcher);

		listener.requestRegister(channel, [&listener](const int &v) { listener.onInt(v); });
		harness.dispatcher.dispatchCommands();

		cge::event::BroadcasterBase broadcaster(&harness.dispatcher);
		broadcaster.broadcast(channel, 1);
		harness.dispatcher.dispatchEvents();
		ASSERT_EQUAL(listener.received.size(), static_cast<size_t>(1));

		ASSERT_TRUE(listener.requestUnregister(channel) == cge::event::RegistrationResult::Pending);
		harness.dispatcher.dispatchCommands();

		broadcaster.broadcast(channel, 2);
		harness.dispatcher.dispatchEvents();
		ASSERT_EQUAL(listener.received.size(), static_cast<size_t>(1));
	}

	void unregisterNotRegisteredStillPendingThenNotFound()
	{
		AsyncEventHarness harness;
		const cge::event::EventChannel<int> &channel = harness.registry.getChannel<int>("reg-missing");
		CountingListener listener(&harness.dispatcher);

		// No register first — unregistration is still queued as a command.
		ASSERT_TRUE(listener.requestUnregister(channel) == cge::event::RegistrationResult::Pending);
		ASSERT_NOTHROW(harness.dispatcher.dispatchCommands());

		// The NotFound outcome must leave the listener fully usable afterwards.
		listener.requestRegister(channel, [&listener](const int &v) { listener.onInt(v); });
		harness.dispatcher.dispatchCommands();

		cge::event::BroadcasterBase broadcaster(&harness.dispatcher);
		broadcaster.broadcast(channel, 3);
		harness.dispatcher.dispatchEvents();
		ASSERT_EQUAL(listener.received.size(), static_cast<size_t>(1));
		ASSERT_EQUAL(listener.received[0], 3);
	}

	// Pending-list only guards duplicates; a second request after finalize may queue
	// again and finalize as Duplicate on the dispatcher map (handler already present).
	void secondRegisterAfterSuccessStillAcceptedAsPending()
	{
		AsyncEventHarness harness;
		const cge::event::EventChannel<int> &channel = harness.registry.getChannel<int>("reg-again");
		CountingListener listener(&harness.dispatcher);

		listener.requestRegister(channel, [&listener](const int &v) { listener.onInt(v); });
		harness.dispatcher.dispatchCommands();

		cge::event::RegistrationResult again = listener.requestRegister(channel, [&listener](const int &v) { listener.onInt(v); });
		ASSERT_TRUE(again == cge::event::RegistrationResult::Pending);
		harness.dispatcher.dispatchCommands();

		cge::event::BroadcasterBase broadcaster(&harness.dispatcher);
		broadcaster.broadcast(channel, 1);
		harness.dispatcher.dispatchEvents();

		// Dispatcher rejects duplicate listener pointer on the channel; only one delivery.
		ASSERT_EQUAL(listener.received.size(), static_cast<size_t>(1));
	}

	// Cycle-order contract: commands drain before events, so an unregistration
	// requested after a broadcast still wins — the queued event never lands.
	void unregistrationBeatsQueuedEvents()
	{
		AsyncEventHarness harness;
		const cge::event::EventChannel<int> &channel = harness.registry.getChannel<int>("unreg-beats");
		CountingListener listener(&harness.dispatcher);

		listener.requestRegister(channel, [&listener](const int &v) { listener.onInt(v); });
		harness.dispatcher.dispatchCommands();

		cge::event::BroadcasterBase broadcaster(&harness.dispatcher);
		broadcaster.broadcast(channel, 1);
		listener.requestUnregister(channel);

		harness.dispatcher.dispatchCommands();
		harness.dispatcher.dispatchEvents();

		ASSERT_EQUAL(listener.received.size(), static_cast<size_t>(0));
	}

	// Swap-and-pop removal must not disturb the remaining registrations. Delivery
	// order among listeners is contract-free, so none is asserted.
	void unregisterOneOfSeveralListenersOthersStillReceive()
	{
		AsyncEventHarness harness;
		const cge::event::EventChannel<int> &channel = harness.registry.getChannel<int>("multi-unreg");
		CountingListener a(&harness.dispatcher);
		CountingListener b(&harness.dispatcher);
		CountingListener c(&harness.dispatcher);

		a.requestRegister(channel, [&a](const int &v) { a.onInt(v); });
		b.requestRegister(channel, [&b](const int &v) { b.onInt(v); });
		c.requestRegister(channel, [&c](const int &v) { c.onInt(v); });
		harness.dispatcher.dispatchCommands();

		b.requestUnregister(channel);
		harness.dispatcher.dispatchCommands();

		cge::event::BroadcasterBase broadcaster(&harness.dispatcher);
		broadcaster.broadcast(channel, 7);
		harness.dispatcher.dispatchEvents();

		ASSERT_EQUAL(a.received.size(), static_cast<size_t>(1));
		ASSERT_EQUAL(b.received.size(), static_cast<size_t>(0));
		ASSERT_EQUAL(c.received.size(), static_cast<size_t>(1));
	}

	void listenerOnTwoChannelsUnregisterOneKeepsOther()
	{
		AsyncEventHarness harness;
		const cge::event::EventChannel<int> &first = harness.registry.getChannel<int>("two-ch-a");
		const cge::event::EventChannel<int> &second = harness.registry.getChannel<int>("two-ch-b");
		CountingListener listener(&harness.dispatcher);

		listener.requestRegister(first, [&listener](const int &v) { listener.onInt(v); });
		listener.requestRegister(second, [&listener](const int &v) { listener.onInt(v); });
		harness.dispatcher.dispatchCommands();

		listener.requestUnregister(first);
		harness.dispatcher.dispatchCommands();

		cge::event::BroadcasterBase broadcaster(&harness.dispatcher);
		broadcaster.broadcast(first, 1);
		broadcaster.broadcast(second, 2);
		harness.dispatcher.dispatchEvents();

		ASSERT_EQUAL(listener.received.size(), static_cast<size_t>(1));
		ASSERT_EQUAL(listener.received[0], 2);
	}

	// One batch, FIFO: register applies, unregister applies, and the re-register
	// was rejected as Duplicate while the first was still pending — so the batch
	// nets out unregistered. A fresh request after the drain succeeds.
	void registerUnregisterRegisterInOneBatch()
	{
		AsyncEventHarness harness;
		const cge::event::EventChannel<int> &channel = harness.registry.getChannel<int>("batch");
		CountingListener listener(&harness.dispatcher);
		cge::event::BroadcasterBase broadcaster(&harness.dispatcher);

		ASSERT_TRUE(listener.requestRegister(channel, [&listener](const int &v) { listener.onInt(v); })
			== cge::event::RegistrationResult::Pending);
		ASSERT_TRUE(listener.requestUnregister(channel) == cge::event::RegistrationResult::Pending);
		ASSERT_TRUE(listener.requestRegister(channel, [&listener](const int &v) { listener.onInt(v); })
			== cge::event::RegistrationResult::Duplicate);

		harness.dispatcher.dispatchCommands();
		broadcaster.broadcast(channel, 1);
		harness.dispatcher.dispatchEvents();
		ASSERT_EQUAL(listener.received.size(), static_cast<size_t>(0));

		ASSERT_TRUE(listener.requestRegister(channel, [&listener](const int &v) { listener.onInt(v); })
			== cge::event::RegistrationResult::Pending);
		harness.dispatcher.dispatchCommands();
		broadcaster.broadcast(channel, 2);
		harness.dispatcher.dispatchEvents();
		ASSERT_EQUAL(listener.received.size(), static_cast<size_t>(1));
		ASSERT_EQUAL(listener.received[0], 2);
	}
};

#endif
