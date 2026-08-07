#include "eventConcurrencyTest.h"

#include <atomic>
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
	}

	EventConcurrencyTest::EventConcurrencyTest(const DispatcherFlavor &flavor)
		: EventLoadSuite("EventConcurrencyTest", "Cross-thread pushes and registration churn.", flavor)
	{
		partest::TestFlags flags = partest::TEST_FLAGS_INHERIT;

		addTest("Threads", flags, [this]() { threads(); });
		addTest("Churn", flags, [this]() { churn(); });
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
