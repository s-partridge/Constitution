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
// AsyncDispatcher
// ---------------------------------------------------------------------------
class AsyncDispatcherTest : public partest::TestBase
{
public:
	AsyncDispatcherTest()
		: TestBase("AsyncDispatcherTest", "Async dispatcher push, drain, and lifecycle.")
	{
		partest::TestFlags flags = partest::TEST_FLAGS_INHERIT;

		addTest("Lifecycle", flags, [this]() { lifecycle(); });
		addTest("Deferral", flags, [this]() { deferral(); });
		addTest("Drain", flags, [this]() { drain(); });
		addTest("Isolation", flags, [this]() { isolation(); });
		addTest("Threads", flags, [this]() { threads(); });
	}

	// Each case needs a dispatcher at a different lifecycle point, so only the
	// registry and its channel identity are shared.
	void lifecycle()
	{
		cge::event::EventChannelRegistry registry;
		const cge::event::EventChannel<int> &channel = registry.getChannel<int>("lifecycle");

		subtest("PushBeforeSetUp", [&]() {
			cge::event::AsyncDispatcher dispatcher("pre-setup", &registry);
			cge::event::ListenerBase listener(&dispatcher);

			ASSERT_TRUE(listener.requestRegister(channel, [](const int &) {})
				== cge::event::RegistrationResult::Failure);
		});

		subtest("PushAfterTearDown", [&]() {
			cge::event::AsyncDispatcher dispatcher("post-teardown", &registry);
			dispatcher.setUp();
			dispatcher.tearDown();

			cge::event::ListenerBase listener(&dispatcher);
			ASSERT_TRUE(listener.requestRegister(channel, [](const int &) {})
				== cge::event::RegistrationResult::Failure);

			cge::event::BroadcasterBase broadcaster(&dispatcher);
			ASSERT_NOTHROW(broadcaster.broadcast(channel, 1));
		});

		// Dispatch always drains, so a parked event would surface on the next
		// drain — nothing arriving proves the push was refused outright.
		subtest("InactivePushDiscarded", [&]() {
			cge::event::AsyncDispatcher dispatcher("inactive-drop", &registry);
			CountingListener listener(&dispatcher);
			cge::event::BroadcasterBase broadcaster(&dispatcher);

			broadcaster.broadcast(channel, 1);

			dispatcher.setUp();
			listener.requestRegister(channel, [&listener](const int &v) { listener.onInt(v); });
			dispatcher.dispatchCommands();
			dispatcher.dispatchEvents();
			ASSERT_EQUAL(listener.received.size(), static_cast<size_t>(0));

			dispatcher.tearDown();
			broadcaster.broadcast(channel, 2);
			dispatcher.dispatchEvents();
			ASSERT_EQUAL(listener.received.size(), static_cast<size_t>(0));
		});

		// tearDown stops intake, not processing: the engine keeps driving dispatch
		// on its own schedule, and work already queued still drains.
		subtest("QueuedEventsDrainAfterTearDown", [&]() {
			cge::event::AsyncDispatcher dispatcher("drain-after-teardown", &registry);
			dispatcher.setUp();

			CountingListener listener(&dispatcher);
			listener.requestRegister(channel, [&listener](const int &v) { listener.onInt(v); });
			dispatcher.dispatchCommands();

			cge::event::BroadcasterBase broadcaster(&dispatcher);
			broadcaster.broadcast(channel, 5);
			dispatcher.tearDown();

			dispatcher.dispatchEvents();
			ASSERT_EQUAL(listener.received.size(), static_cast<size_t>(1));
			ASSERT_EQUAL(listener.received[0], 5);
		});
	}

	// One listener carried from request through to delivery: each case runs
	// against the dispatcher state the previous one left behind.
	void deferral()
	{
		AsyncEventHarness harness;
		const cge::event::EventChannel<int> &channel = harness.registry.getChannel<int>("deferral");
		CountingListener listener(&harness.dispatcher);
		cge::event::BroadcasterBase broadcaster(&harness.dispatcher);

		subtest("EmptyQueuesNoOp", [&]() {
			ASSERT_NOTHROW(harness.dispatcher.dispatchCommands());
			ASSERT_NOTHROW(harness.dispatcher.dispatchEvents());
		});

		subtest("RegistrationWaitsForCommandDrain", [&]() {
			ASSERT_TRUE(listener.requestRegister(channel, [&listener](const int &v) { listener.onInt(v); })
				== cge::event::RegistrationResult::Pending);

			// Listener map not updated yet, so this event drains to nobody.
			broadcaster.broadcast(channel, 7);
			harness.dispatcher.dispatchEvents();
			ASSERT_EQUAL(listener.received.size(), static_cast<size_t>(0));
		});

		subtest("EventsWaitForEventDrain", [&]() {
			harness.dispatcher.dispatchCommands();

			broadcaster.broadcast(channel, 42);
			ASSERT_EQUAL(listener.received.size(), static_cast<size_t>(0));

			harness.dispatcher.dispatchEvents();
			ASSERT_EQUAL(listener.received.size(), static_cast<size_t>(1));
			ASSERT_EQUAL(listener.received[0], 42);
		});
	}

	// Shared dispatcher; what varies is what the handler does during the drain.
	void drain()
	{
		AsyncEventHarness harness;
		cge::event::BroadcasterBase broadcaster(&harness.dispatcher);

		subtest("ReentryBroadcasts", [&]() {
			const cge::event::EventChannel<int> &channel = harness.registry.getChannel<int>("reentry");
			CountingListener listener(&harness.dispatcher);

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
		});

		// Pending semantics: an unregistration requested mid-drain takes effect at
		// the next command drain, so the rest of this drain still delivers.
		subtest("UnregisterMidDrain", [&]() {
			const cge::event::EventChannel<int> &channel = harness.registry.getChannel<int>("same-drain");
			CountingListener listener(&harness.dispatcher);

			listener.requestRegister(channel, [&](const int &v) {
				listener.onInt(v);
				if(v == 1)
					listener.requestUnregister(channel);
			});
			harness.dispatcher.dispatchCommands();

			broadcaster.broadcast(channel, 1);
			broadcaster.broadcast(channel, 2);
			broadcaster.broadcast(channel, 3);
			harness.dispatcher.dispatchEvents();
			ASSERT_EQUAL(listener.received.size(), static_cast<size_t>(3));

			harness.dispatcher.dispatchCommands();
			broadcaster.broadcast(channel, 4);
			harness.dispatcher.dispatchEvents();
			ASSERT_EQUAL(listener.received.size(), static_cast<size_t>(3));
		});
	}

	void isolation()
	{
		subtest("NonCommandChannelRejected", [&]() {
			AsyncEventHarness harness;
			const cge::event::EventChannel<int> &channel = harness.registry.getChannel<int>("cmd-only");
			CountingListener listener(&harness.dispatcher);

			listener.requestRegister(channel, [&listener](const int &v) { listener.onInt(v); });
			harness.dispatcher.dispatchCommands();

			cge::event::CommanderBase commander(&harness.dispatcher);
			ASSERT_FALSE(commander.command(channel, 99));

			harness.dispatcher.dispatchCommands();
			harness.dispatcher.dispatchEvents();
			ASSERT_EQUAL(listener.received.size(), static_cast<size_t>(0));
		});

		// Two dispatchers share the registry's named registration channels, but
		// queues are per-dispatcher, so traffic must not cross.
		subtest("TwoDispatchersOneRegistry", [&]() {
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
		});
	}

	void threads()
	{
		// Producers join before the drain, so this never overlaps push with
		// dispatch; push-during-drain coverage lives in the load tests.
		subtest("JoinedProducers", [&]() {
			AsyncEventHarness harness;
			const cge::event::EventChannel<int> &channel = harness.registry.getChannel<int>("mt");
			std::atomic<int> total(0);

			cge::event::ListenerBase listener(&harness.dispatcher);
			listener.requestRegister(channel, [&total](const int &v) { total.fetch_add(v); });
			harness.dispatcher.dispatchCommands();

			cge::event::BroadcasterBase broadcaster(&harness.dispatcher);
			const unsigned producers = smokeProducerCount();
			const int perProducer = 50;

			std::vector<std::thread> workers;
			for(unsigned p = 0; p < producers; ++p)
			{
				workers.emplace_back([&broadcaster, &channel, perProducer]() {
					for(int i = 0; i < perProducer; ++i)
						broadcaster.broadcast(channel, 1);
				});
			}
			for(std::thread &t : workers)
				t.join();

			harness.dispatcher.dispatchEvents();

			ASSERT_EQUAL(total.load(), static_cast<int>(producers) * perProducer);
		});
	}

private:
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

		addTest("FrameGated", flags, [this]() { frameGated(); });
		addTest("Continuous", flags, [this]() { continuous(); });
		addTest("RegistrationUnderLoad", flags, [this]() { registrationUnderLoad(); });
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

	// Each case owns its own dispatcher, workers and payload logs — these are
	// independent heavyweight runs, grouped only by production mode.
	void frameGated()
	{
		subtest("Workers", [&]() { frameGatedWorkers(); });
		subtest("Cascade", [&]() { frameGatedCascade(); });
		subtest("MixedEventAndCommand", [&]() { frameGatedMixedEventAndCommand(); });
	}

	void continuous()
	{
		subtest("Workers", [&]() { continuousWorkers(); });
		subtest("Cascade", [&]() { continuousCascade(); });
	}

	void registrationUnderLoad()
	{
		subtest("DuringFlush", [&]() { registerDuringFlush(); });
		subtest("ConcurrentChurn", [&]() { concurrentChurn(); });
	}

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

private:
	// --- Frame-gated: persistent workers, semaphore between produce and drain -----------

	void frameGatedWorkers()
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

	void frameGatedCascade()
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

	void frameGatedMixedEventAndCommand()
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

	void continuousWorkers()
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

	void continuousCascade()
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

	// --- Registration traffic mixed into a running dispatcher -------------------------

	void registerDuringFlush()
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
	void concurrentChurn()
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

		addTest("Delivery", flags, [this]() { delivery(); });
		addTest("PayloadTypes", flags, [this]() { payloadTypes(); });
		addTest("PushResult", flags, [this]() { pushResult(); });
	}

	// broadcast reports whether the dispatcher accepted the push. A refused push
	// is discarded, so the caller's return value is the only signal it gets.
	void pushResult()
	{
		cge::event::EventChannelRegistry registry;
		cge::event::AsyncDispatcher dispatcher("bc-result", &registry);
		const cge::event::EventChannel<int> &channel = registry.getChannel<int>("bc-result-ch");
		cge::event::BroadcasterBase broadcaster(&dispatcher);

		subtest("FailsBeforeSetUp", [&]() {
			ASSERT_FALSE(broadcaster.broadcast(channel, 1));
		});

		subtest("SucceedsWhileActive", [&]() {
			dispatcher.setUp();
			ASSERT_TRUE(broadcaster.broadcast(channel, 2));
		});

		subtest("FailsAfterTearDown", [&]() {
			dispatcher.tearDown();
			ASSERT_FALSE(broadcaster.broadcast(channel, 3));
		});
	}

	// One listener accumulating across the cases; each asserts against the
	// running total rather than rebuilding the fixture.
	void delivery()
	{
		AsyncEventHarness harness;
		const cge::event::EventChannel<int> &channel = harness.registry.getChannel<int>("bc-int");
		CountingListener listener(&harness.dispatcher);

		listener.requestRegister(channel, [&listener](const int &v) { listener.onInt(v); });
		harness.dispatcher.dispatchCommands();
		cge::event::BroadcasterBase broadcaster(&harness.dispatcher);

		subtest("TypedPayload", [&]() {
			broadcaster.broadcast(channel, 123);
			harness.dispatcher.dispatchEvents();

			ASSERT_EQUAL(listener.received.size(), static_cast<size_t>(1));
			ASSERT_EQUAL(listener.received[0], 123);
		});

		subtest("OrderPreserved", [&]() {
			broadcaster.broadcast(channel, 1);
			broadcaster.broadcast(channel, 2);
			broadcaster.broadcast(channel, 3);
			harness.dispatcher.dispatchEvents();

			ASSERT_EQUAL(listener.received.size(), static_cast<size_t>(4));
			ASSERT_EQUAL(listener.received[1], 1);
			ASSERT_EQUAL(listener.received[2], 2);
			ASSERT_EQUAL(listener.received[3], 3);
		});

		subtest("NoListeners", [&]() {
			const cge::event::EventChannel<int> &empty = harness.registry.getChannel<int>("bc-empty");

			ASSERT_NOTHROW(broadcaster.broadcast(empty, 1));
			ASSERT_NOTHROW(harness.dispatcher.dispatchEvents());
		});
	}

	// One dispatcher, one channel per payload category: what varies is the
	// payload type, not the plumbing.
	void payloadTypes()
	{
		AsyncEventHarness harness;
		cge::event::BroadcasterBase broadcaster(&harness.dispatcher);

		subtest("Enum", [&]() {
			enum class GameState { Menu, Loading, Playing };

			const cge::event::EventChannel<GameState> &channel = harness.registry.getChannel<GameState>("bc-enum");
			GameState got = GameState::Menu;

			cge::event::ListenerBase listener(&harness.dispatcher);
			listener.requestRegister(channel, [&got](const GameState &s) { got = s; });
			harness.dispatcher.dispatchCommands();

			broadcaster.broadcast(channel, GameState::Playing);
			harness.dispatcher.dispatchEvents();

			ASSERT_TRUE(got == GameState::Playing);
		});

		// Pointer payloads copy the address, not the pointee: the handler must see
		// the same object the broadcaster pointed at.
		subtest("Pointer", [&]() {
			const cge::event::EventChannel<int*> &channel = harness.registry.getChannel<int*>("bc-ptr");
			int target = 41;
			int *got = nullptr;

			cge::event::ListenerBase listener(&harness.dispatcher);
			listener.requestRegister(channel, [&got](int *const &p) { got = p; });
			harness.dispatcher.dispatchCommands();

			broadcaster.broadcast(channel, &target);
			harness.dispatcher.dispatchEvents();

			ASSERT_TRUE(got == &target);
			*got = 42;
			ASSERT_EQUAL(target, 42);
		});

		// Trivially-copyable aggregate — the archetypal game payload.
		subtest("TrivialStruct", [&]() {
			struct DamagePayload
			{
				int amount;
				float multiplier;
				unsigned sourceId;
			};
			static_assert(std::is_trivially_copyable<DamagePayload>::value,
				"representative must actually belong to the trivially-copyable class");

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

			broadcaster.broadcast(channel, payload);
			harness.dispatcher.dispatchEvents();

			ASSERT_EQUAL(got.amount, 25);
			ASSERT_EQUAL(got.multiplier, 1.5f);
			ASSERT_EQUAL(got.sourceId, 7u);
		});

		// Copy allocates: proves the deep copy survives the queue.
		subtest("Class", [&]() {
			const cge::event::EventChannel<std::string> &channel = harness.registry.getChannel<std::string>("bc-str");
			std::string got;

			cge::event::ListenerBase listener(&harness.dispatcher);
			listener.requestRegister(channel, [&got](const std::string &s) { got = s; });
			harness.dispatcher.dispatchCommands();

			broadcaster.broadcast(channel, std::string("hello-event"));
			harness.dispatcher.dispatchEvents();

			ASSERT_EQUAL(got, std::string("hello-event"));
		});

		// Aggregate whose members own resources: copy is member-wise and
		// non-trivial — distinct from both cases above.
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

			cge::event::ListenerBase listener(&harness.dispatcher);
			listener.requestRegister(channel, [&got](const SpawnRequest &r) { got = r; });
			harness.dispatcher.dispatchCommands();

			SpawnRequest request;
			request.unitType = 3;
			request.name = "archer";
			request.inventory.push_back(10);
			request.inventory.push_back(20);

			broadcaster.broadcast(channel, request);
			harness.dispatcher.dispatchEvents();

			ASSERT_EQUAL(got.unitType, 3);
			ASSERT_EQUAL(got.name, std::string("archer"));
			ASSERT_EQUAL(got.inventory.size(), static_cast<size_t>(2));
			ASSERT_EQUAL(got.inventory[0], 10);
			ASSERT_EQUAL(got.inventory[1], 20);
		});
	}
};

// ---------------------------------------------------------------------------
// CommanderBase
//
// Current design: dispatchCommands only applies registration/unregistration.
// Arbitrary CommanderBase payloads are dequeued and discarded.
// ---------------------------------------------------------------------------
class CommanderBaseTest : public partest::TestBase
{
public:
	CommanderBaseTest()
		: TestBase("CommanderBaseTest", "Public commander enqueue behavior.")
	{
		partest::TestFlags flags = partest::TEST_FLAGS_INHERIT;

		addTest("Command", flags, [this]() { command(); });
		addTest("PushResult", flags, [this]() { pushResult(); });
	}

	// command reports whether the dispatcher accepted the push, same as broadcast.
	void pushResult()
	{
		cge::event::EventChannelRegistry registry;
		cge::event::AsyncDispatcher dispatcher("cmd-result", &registry);
		const cge::event::EventChannel<int> &channel = registry.getChannel<int>("cmd-result-ch");
		cge::event::CommanderBase commander(&dispatcher);

		subtest("FailsBeforeSetUp", [&]() {
			ASSERT_FALSE(commander.command(channel, 1));
		});

		// TODO: no valid command exists to push. The dispatcher validates commands,
		// and the only ones it accepts are registration/unregistration, which come
		// from ListenerBase and never through CommanderBase. Fill this in when a
		// command vocabulary exists that a caller can legitimately send.
		subtest("SucceedsWhileActive", partest::TEST_FLAGS_SKIP, [&]() {
		});

		subtest("FailsAfterTearDown", [&]() {
			dispatcher.setUp();
			dispatcher.tearDown();
			ASSERT_FALSE(commander.command(channel, 3));
		});
	}

	void command()
	{
		subtest("NotDeliveredToListeners", [&]() {
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
		});

		// Drain commands first (drops the command payload), then events.
		subtest("NoCrossoverToEvents", [&]() {
			AsyncEventHarness harness;
			const cge::event::EventChannel<int> &channel = harness.registry.getChannel<int>("cmd-vs-evt");
			CountingListener listener(&harness.dispatcher);

			listener.requestRegister(channel, [&listener](const int &v) { listener.onInt(v); });
			harness.dispatcher.dispatchCommands();

			cge::event::CommanderBase commander(&harness.dispatcher);
			cge::event::BroadcasterBase broadcaster(&harness.dispatcher);

			commander.command(channel, 1);
			broadcaster.broadcast(channel, 2);
			harness.dispatcher.dispatchCommands();
			harness.dispatcher.dispatchEvents();

			ASSERT_EQUAL(listener.received.size(), static_cast<size_t>(1));
			ASSERT_EQUAL(listener.received[0], 2);
		});

		subtest("AfterTearDown", [&]() {
			cge::event::EventChannelRegistry registry;
			cge::event::AsyncDispatcher dispatcher("cmd-td", &registry);
			dispatcher.setUp();
			const cge::event::EventChannel<int> &channel = registry.getChannel<int>("cmd-td-ch");
			cge::event::CommanderBase commander(&dispatcher);
			dispatcher.tearDown();

			ASSERT_NOTHROW(commander.command(channel, 1));
		});
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

		addTest("RegistrationLifecycle", flags, [this]() { registrationLifecycle(); });
		addTest("Unregister", flags, [this]() { unregister(); });
		addTest("BatchedRequests", flags, [this]() { batchedRequests(); });
		addTest("Handlers", flags, [this]() { handlers(); });
	}

	// One listener walked through its whole registration life on a single
	// dispatcher; the cases run in order and share that accumulated state.
	void registrationLifecycle()
	{
		AsyncEventHarness harness;
		const cge::event::EventChannel<int> &channel = harness.registry.getChannel<int>("reg-life");
		CountingListener listener(&harness.dispatcher);
		cge::event::BroadcasterBase broadcaster(&harness.dispatcher);
		auto handler = [&listener](const int &v) { listener.onInt(v); };

		subtest("RequestReturnsPending", [&]() {
			ASSERT_TRUE(listener.requestRegister(channel, handler)
				== cge::event::RegistrationResult::Pending);
		});

		subtest("DuplicateWhilePending", [&]() {
			ASSERT_TRUE(listener.requestRegister(channel, handler)
				== cge::event::RegistrationResult::Duplicate);
		});

		subtest("DeliversAfterCommandDrain", [&]() {
			harness.dispatcher.dispatchCommands();
			broadcaster.broadcast(channel, 1);
			harness.dispatcher.dispatchEvents();

			ASSERT_EQUAL(listener.received.size(), static_cast<size_t>(1));
			ASSERT_EQUAL(listener.received[0], 1);
		});

		// The pending list only guards duplicates, so a request after finalize is
		// accepted; the dispatcher then rejects it, leaving delivery single.
		subtest("RequestAgainAfterSuccess", [&]() {
			ASSERT_TRUE(listener.requestRegister(channel, handler)
				== cge::event::RegistrationResult::Pending);
			harness.dispatcher.dispatchCommands();

			broadcaster.broadcast(channel, 2);
			harness.dispatcher.dispatchEvents();

			ASSERT_EQUAL(listener.received.size(), static_cast<size_t>(2));
			ASSERT_EQUAL(listener.received[1], 2);
		});

		subtest("UnregisterStopsDelivery", [&]() {
			ASSERT_TRUE(listener.requestUnregister(channel) == cge::event::RegistrationResult::Pending);
			harness.dispatcher.dispatchCommands();

			broadcaster.broadcast(channel, 3);
			harness.dispatcher.dispatchEvents();

			ASSERT_EQUAL(listener.received.size(), static_cast<size_t>(2));
		});
	}

	// Shared dispatcher; each case brings its own channels and listeners.
	void unregister()
	{
		AsyncEventHarness harness;
		cge::event::BroadcasterBase broadcaster(&harness.dispatcher);

		// Cycle order: commands drain before events, so an unregistration
		// requested after a broadcast still wins.
		subtest("BeatsQueuedEvents", [&]() {
			const cge::event::EventChannel<int> &channel = harness.registry.getChannel<int>("unreg-beats");
			CountingListener listener(&harness.dispatcher);

			listener.requestRegister(channel, [&listener](const int &v) { listener.onInt(v); });
			harness.dispatcher.dispatchCommands();

			broadcaster.broadcast(channel, 1);
			listener.requestUnregister(channel);

			harness.dispatcher.dispatchCommands();
			harness.dispatcher.dispatchEvents();

			ASSERT_EQUAL(listener.received.size(), static_cast<size_t>(0));
		});

		// Swap-and-pop removal must not disturb the remaining registrations.
		// Delivery order among listeners is contract-free, so none is asserted.
		subtest("OneOfSeveralListeners", [&]() {
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

			broadcaster.broadcast(channel, 7);
			harness.dispatcher.dispatchEvents();

			ASSERT_EQUAL(a.received.size(), static_cast<size_t>(1));
			ASSERT_EQUAL(b.received.size(), static_cast<size_t>(0));
			ASSERT_EQUAL(c.received.size(), static_cast<size_t>(1));
		});

		subtest("OneOfTwoChannels", [&]() {
			const cge::event::EventChannel<int> &first = harness.registry.getChannel<int>("two-ch-a");
			const cge::event::EventChannel<int> &second = harness.registry.getChannel<int>("two-ch-b");
			CountingListener listener(&harness.dispatcher);

			listener.requestRegister(first, [&listener](const int &v) { listener.onInt(v); });
			listener.requestRegister(second, [&listener](const int &v) { listener.onInt(v); });
			harness.dispatcher.dispatchCommands();

			listener.requestUnregister(first);
			harness.dispatcher.dispatchCommands();

			broadcaster.broadcast(first, 1);
			broadcaster.broadcast(second, 2);
			harness.dispatcher.dispatchEvents();

			ASSERT_EQUAL(listener.received.size(), static_cast<size_t>(1));
			ASSERT_EQUAL(listener.received[0], 2);
		});

		// Never registered: the request still queues, and the NotFound outcome
		// must leave the listener fully usable afterwards.
		subtest("NotRegistered", [&]() {
			const cge::event::EventChannel<int> &channel = harness.registry.getChannel<int>("reg-missing");
			CountingListener listener(&harness.dispatcher);

			ASSERT_TRUE(listener.requestUnregister(channel) == cge::event::RegistrationResult::Pending);
			ASSERT_NOTHROW(harness.dispatcher.dispatchCommands());

			listener.requestRegister(channel, [&listener](const int &v) { listener.onInt(v); });
			harness.dispatcher.dispatchCommands();

			broadcaster.broadcast(channel, 3);
			harness.dispatcher.dispatchEvents();
			ASSERT_EQUAL(listener.received.size(), static_cast<size_t>(1));
			ASSERT_EQUAL(listener.received[0], 3);
		});
	}

	// One command batch, drained FIFO, then the recovery afterwards.
	void batchedRequests()
	{
		AsyncEventHarness harness;
		const cge::event::EventChannel<int> &channel = harness.registry.getChannel<int>("batch");
		CountingListener listener(&harness.dispatcher);
		cge::event::BroadcasterBase broadcaster(&harness.dispatcher);
		auto handler = [&listener](const int &v) { listener.onInt(v); };

		// The caller asked to end up registered, so it must end up registered and
		// receiving. Currently fails: the re-register is rejected as Duplicate
		// against the still-pending first request and queues nothing.
		subtest("EndsUpRegistered", [&]() {
			ASSERT_TRUE(listener.requestRegister(channel, handler)
				== cge::event::RegistrationResult::Pending);
			ASSERT_TRUE(listener.requestUnregister(channel)
				== cge::event::RegistrationResult::Pending);
			ASSERT_TRUE(listener.requestRegister(channel, handler)
				== cge::event::RegistrationResult::Pending);

			harness.dispatcher.dispatchCommands();
			broadcaster.broadcast(channel, 1);
			harness.dispatcher.dispatchEvents();

			ASSERT_EQUAL(listener.received.size(), static_cast<size_t>(1));
			if(listener.received.size() == 1)
				ASSERT_EQUAL(listener.received[0], 1);
		});

	}

	// Shared dispatcher; the callback form and the listener count are what vary.
	void handlers()
	{
		AsyncEventHarness harness;
		cge::event::BroadcasterBase broadcaster(&harness.dispatcher);

		subtest("MemberFunctionForm", [&]() {
			const cge::event::EventChannel<int> &channel = harness.registry.getChannel<int>("reg-member");
			CountingListener listener(&harness.dispatcher);

			ASSERT_TRUE(listener.requestRegister(channel, &listener, &CountingListener::onInt)
				== cge::event::RegistrationResult::Pending);
			harness.dispatcher.dispatchCommands();

			broadcaster.broadcast(channel, 77);
			harness.dispatcher.dispatchEvents();

			ASSERT_EQUAL(listener.received.size(), static_cast<size_t>(1));
			ASSERT_EQUAL(listener.received[0], 77);
		});

		subtest("MultipleListeners", [&]() {
			const cge::event::EventChannel<int> &channel = harness.registry.getChannel<int>("reg-multi");
			CountingListener a(&harness.dispatcher);
			CountingListener b(&harness.dispatcher);

			a.requestRegister(channel, [&a](const int &v) { a.onInt(v); });
			b.requestRegister(channel, [&b](const int &v) { b.onInt(v); });
			harness.dispatcher.dispatchCommands();

			broadcaster.broadcast(channel, 9);
			harness.dispatcher.dispatchEvents();

			ASSERT_EQUAL(a.received.size(), static_cast<size_t>(1));
			ASSERT_EQUAL(b.received.size(), static_cast<size_t>(1));
			ASSERT_EQUAL(a.received[0], 9);
			ASSERT_EQUAL(b.received[0], 9);
		});
	}
};

#endif
