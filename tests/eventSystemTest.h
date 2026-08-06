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

	// Each case owns its own dispatcher, workers and payload logs. These are
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

#endif
