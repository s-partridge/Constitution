#include "eventConcurrencyTest.h"

#include <atomic>
#include <chrono>
#include <memory>
#include <thread>
#include <vector>

#include <partest/assert.h>

#include "broadcaster.h"

namespace cge::test
{
	namespace
	{
		// Batch size for churn payload packing. Keeps sequence values inside the
		// field makePayload reserves for them.
		const unsigned kPushesPerBatch = 512;

		// The liveness probe finishes in well under a second when the locks are
		// sound. Anything near this bound is already a wedge.
		const std::chrono::seconds kLivenessDeadline(30);
	}

	EventConcurrencyTest::EventConcurrencyTest(const DispatcherFlavor &flavor)
		: EventLoadSuite("EventConcurrencyTest", "Cross-thread pushes and registration churn.", flavor)
	{
		partest::TestFlags flags = partest::TEST_FLAGS_INHERIT;

		// First, and it stops the rest of the suite if it fails. Everything below
		// assumes the dispatcher makes progress under contention.
		addTest("Liveness", flags.withStopOnFail(partest::FlagState::Enabled), [this]() { liveness(); });
		addTest("Threads", flags, [this]() { threads(); });
		addTest("Churn", flags, [this]() { churn(); });
	}

	// Liveness, and nothing else. No payload is checked and no delivery is
	// asserted; the only question is whether the run finishes at all.
	//
	// The contract: pushes and registration requests arrive from any thread at
	// any time, dispatch runs on the processing thread, and the system keeps
	// making progress while both are happening. Every other concurrency test
	// assumes that and measures something else. This one asserts it directly, by
	// putting registration traffic, event traffic and both drains in flight
	// against the same listeners at once and requiring the run to end.
	//
	// stopOnFail because everything downstream assumes progress. If the system
	// can stop making it, later results are measuring a dispatcher that has
	// already violated the thing they depend on.
	//
	// Isolated because a wedge cannot be released. There is no timed join and no
	// way to signal a thread that is not looking, so the run owns its harness,
	// channels, listeners and threads, and on timeout the test abandons the lot
	// and reports rather than hanging the executable. Nothing it can still reach
	// belongs to this function.
	void EventConcurrencyTest::liveness()
	{
		const char *label = "EventConcurrencyTest.Liveness";

		const IsolatedOutcome<bool> outcome = runIsolated<bool>(label, kLivenessDeadline, [this]() {
			EventHarness harness(flavor(), "liveness-dispatcher");
			const cge::event::EventChannel<int> &channel = harness.registry.getChannel<int>("liveness");

			const unsigned churners = 4;
			const unsigned producers = 2;
			const unsigned churnCycles = 2048;
			const unsigned pushesPerProducer = 8192;
			std::atomic<unsigned> running(churners + producers);

			std::vector<std::unique_ptr<cge::event::ListenerBase>> listeners;
			listeners.reserve(churners);
			for(unsigned c = 0; c < churners; ++c)
				listeners.push_back(std::unique_ptr<cge::event::ListenerBase>(
					new cge::event::ListenerBase(&harness.dispatcher())));

			cge::event::BroadcasterBase broadcaster(&harness.dispatcher());

			std::vector<std::thread> threads;
			threads.reserve(churners + producers);

			// No yielding, unlike ConcurrentChurn. Requests should be arriving
			// while the drain is running, not politely between drains.
			for(unsigned c = 0; c < churners; ++c)
			{
				cge::event::ListenerBase *churner = listeners[c].get();
				threads.emplace_back([&running, &channel, churner, churnCycles]() {
					for(unsigned i = 0; i < churnCycles; ++i)
					{
						churner->requestRegister(channel, [](const int &) {});
						churner->requestUnregister(channel);
					}
					running.fetch_sub(1);
				});
			}
			for(unsigned p = 0; p < producers; ++p)
			{
				threads.emplace_back([&running, &broadcaster, &channel, pushesPerProducer]() {
					for(unsigned i = 0; i < pushesPerProducer; ++i)
						broadcaster.broadcast(channel, 1);
					running.fetch_sub(1);
				});
			}

			// Both drains run against the listeners that are being churned, for
			// as long as the churn lasts.
			while(running.load() > 0)
			{
				harness.dispatcher().dispatchCommands();
				harness.dispatcher().dispatchEvents();
			}

			for(std::thread &t : threads)
				t.join();

			return true;
		});

		ASSERT_TRUE(outcome.completed);
	}

	void EventConcurrencyTest::threads()
	{
		// Producers join before the drain, so this never overlaps push with
		// dispatch; push-during-drain coverage lives in the load suite.
		subtest("JoinedProducers", [&]() {
			EventHarness harness(flavor(), "mt-dispatcher");
			const cge::event::EventChannel<int> &channel = harness.registry.getChannel<int>("mt");
			std::atomic<int> total(0);

			cge::event::ListenerBase listener(&harness.dispatcher());
			listener.requestRegister(channel, [&total](const int &v) { total.fetch_add(v); });
			harness.dispatcher().dispatchCommands();

			cge::event::BroadcasterBase broadcaster(&harness.dispatcher());
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

			harness.dispatcher().dispatchEvents();

			ASSERT_EQUAL(total.load(), static_cast<int>(producers) * perProducer);
		});
	}

	// Registration churn from worker threads while producers broadcast. This is
	// the scenario the pending-handler mutex exists for. Only determinism-proof
	// invariants are asserted: the stable listener sees every payload, and
	// nothing crashes. What the churning listeners receive depends on drain
	// timing and is deliberately unasserted.
	void EventConcurrencyTest::churn()
	{
		subtest("ConcurrentChurn", [&]() {
			EventHarness harness(flavor(), "churn-dispatcher");
			const cge::event::EventChannel<int> &channel = harness.registry.getChannel<int>("churn");
			PayloadLog sent;
			PayloadLog received;

			cge::event::ListenerBase stable(&harness.dispatcher());
			stable.requestRegister(channel, [&received](const int &v) { received.record(v); });
			harness.dispatcher().dispatchCommands();

			cge::event::BroadcasterBase broadcaster(&harness.dispatcher());
			const unsigned producers = 2;
			const unsigned churners = 2;
			const unsigned perProducer = kPushesPerBatch * 8;
			const unsigned churnCycles = 256;
			std::atomic<unsigned> runningWorkers(producers + churners);

			std::vector<std::unique_ptr<cge::event::ListenerBase>> churnListeners;
			for(unsigned c = 0; c < churners; ++c)
				churnListeners.push_back(std::unique_ptr<cge::event::ListenerBase>(
					new cge::event::ListenerBase(&harness.dispatcher())));

			std::vector<std::thread> threads;
			for(unsigned p = 0; p < producers; ++p)
			{
				threads.emplace_back([&, p]() {
					for(unsigned i = 0; i < perProducer; ++i)
					{
						const int payload = makePayload(i / kPushesPerBatch, p, i % kPushesPerBatch);
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
				harness.dispatcher().dispatchCommands();
				harness.dispatcher().dispatchEvents();
			}
			for(std::thread &t : threads)
				t.join();

			// Trailing drains: apply any residual commands, then flush remaining events.
			for(unsigned extra = 0; extra < 4; ++extra)
			{
				harness.dispatcher().dispatchCommands();
				harness.dispatcher().dispatchEvents();
			}

			assertPayloadsPreserved(sent.snapshot(), received.snapshot());
		});
	}
}
