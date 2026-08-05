#ifndef EVENT_SYSTEM_TEST_H
#define EVENT_SYSTEM_TEST_H

#include <algorithm>
#include <atomic>
#include <mutex>
#include <semaphore>
#include <string>
#include <thread>
#include <type_traits>
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
		addTest("ConcurrentProducersAreDeliveredOnDispatch", flags, [this]() { concurrentProducersAreDeliveredOnDispatch(); });
		addTest("UnknownCommandsAreConsumedWithoutListenerDelivery", flags, [this]() { unknownCommandsAreConsumedWithoutListenerDelivery(); });
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

	void concurrentProducersAreDeliveredOnDispatch()
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
	void assertPayloadsPreserved(const std::vector<int> &sent, const std::vector<int> &received)
	{
		ASSERT_EQUAL(received.size(), sent.size());

		std::vector<int> expected = sent;
		std::vector<int> actual = received;
		std::sort(expected.begin(), expected.end());
		std::sort(actual.begin(), actual.end());

		unsigned mismatches = 0;
		for(size_t i = 0; i < expected.size(); ++i)
		{
			if(expected[i] != actual[i])
				++mismatches;
		}
		ASSERT_EQUAL(mismatches, 0u);
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

		runPersistentFrameGated(workers, kFrameCount, kPushesPerWorkerPerFrame,
			[&](unsigned frame, unsigned worker, unsigned seq) {
				const int payload = makePayload(frame, worker, seq);
				sent.record(payload);
				broadcaster.broadcast(channel, payload);
			},
			[&]() {
				harness.dispatcher.dispatchEvents();
			});

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

		runPersistentFrameGated(workers, kFrameCount, perWorker,
			[&](unsigned frame, unsigned worker, unsigned seq) {
				const int payload = makePayload(frame, worker, seq);
				sent.record(payload);
				broadcaster.broadcast(primary, payload);
			},
			[&]() {
				harness.dispatcher.dispatchEvents();
			});

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

		runPersistentFrameGated(workers, kFrameCount, kPushesPerWorkerPerFrame,
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

private:
	// Persistent workers. Each frame: main releases all workers, waits for all to finish
	// producing, then runs onFrameComplete (dispatch). Same gate idea as a job fence.
	template<typename ProduceFn, typename FrameCompleteFn>
	static void runPersistentFrameGated(
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

		for(unsigned frame = 0; frame < frameCount; ++frame)
		{
			currentFrame.store(frame, std::memory_order_release);
			for(unsigned w = 0; w < workers; ++w)
				beginFrame.release();
			for(unsigned w = 0; w < workers; ++w)
				endFrame.acquire();

			onFrameComplete();
		}

		stop.store(true, std::memory_order_release);
		for(unsigned w = 0; w < workers; ++w)
			beginFrame.release();
		for(std::thread &t : threads)
			t.join();
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
		addTest("StringPayloadDoesNotSlice", flags, [this]() { stringPayloadDoesNotSlice(); });
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

	void stringPayloadDoesNotSlice()
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
		cge::event::ListenerBase listener(&harness.dispatcher);

		// No register first — unregistration is still queued as a command.
		ASSERT_TRUE(listener.requestUnregister(channel) == cge::event::RegistrationResult::Pending);
		ASSERT_NOTHROW(harness.dispatcher.dispatchCommands());
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
};

#endif
