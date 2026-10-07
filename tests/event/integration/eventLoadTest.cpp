#include "eventLoadTest.h"

#include <memory>
#include <thread>
#include <vector>

#include <partest/assert.h>

#include "broadcaster.h"

namespace cge::test
{
	namespace
	{
		const unsigned pushesPerWorkerPerFrame = 512;
		const unsigned frameCount = 32;
	}

	EventLoadTest::EventLoadTest(const DispatcherFlavor &flavor)
		: EventLoadSuite("EventLoadTest", "Multi-frame loads with payload preservation checks.", flavor)
	{
		partest::TestFlags flags = partest::TEST_FLAGS_INHERIT;

		addTest("FrameGated", flags, PARTEST_CTX(this) { frameGated(ctx); });
		addTest("Continuous", flags, PARTEST_CTX(this) { continuous(ctx); });
	}

	// Each case owns its own dispatcher, workers and payload logs. These are
	// independent heavyweight runs, grouped only by production mode.
	void EventLoadTest::frameGated(partest::TestContext &ctx)
	{
		ctx.subtest("Workers", PARTEST_CTX(&) { frameGatedWorkers(ctx); });
		ctx.subtest("Cascade", PARTEST_CTX(&) { frameGatedCascade(ctx); });
		ctx.subtest("Churn", PARTEST_CTX(&) { frameGatedChurn(ctx); });
	}

	void EventLoadTest::continuous(partest::TestContext &ctx)
	{
		ctx.subtest("Workers", PARTEST_CTX(&) { continuousWorkers(ctx); });
		ctx.subtest("Cascade", PARTEST_CTX(&) { continuousCascade(ctx); });
	}

	// --- Frame-gated: persistent workers, semaphore between produce and drain ---

	void EventLoadTest::frameGatedWorkers(partest::TestContext &ctx)
	{
		EventHarness harness(flavor(), "load-gated-dispatcher");
		const cge::event::EventChannel<LoadPayload> &channel = harness.registry.getChannel<LoadPayload>("load-gated");
		PayloadLog sent;
		PayloadLog received;

		cge::event::ListenerBase listener(&harness.dispatcher());
		listener.requestRegister(channel, [&received](const LoadPayload &v) { received.record(v); });
		harness.dispatcher().dispatchCommands();

		cge::event::BroadcasterBase broadcaster(&harness.dispatcher());
		const unsigned workers = loadWorkerCount();

		const bool completed = runPersistentFrameGated("EventLoadTest.FrameGated.Workers",
			workers, frameCount, pushesPerWorkerPerFrame,
			[&](unsigned frame, unsigned worker, unsigned seq) {
				const LoadPayload payload(frame, worker, seq);
				sent.record(payload);
				broadcaster.broadcast(channel, payload);
			},
			[&]() {
				harness.dispatcher().dispatchEvents();
			});
		ASSERT_TRUE(completed);
		if(!completed)
			return;

		// Set equality is all cross-producer order requires. Per-producer order is
		// a contract on top of that, checked per worker so the interleaving
		// between workers stays irrelevant.
		assertPayloadsPreserved(ctx, sent.snapshot(), received.snapshot());
		assertProducerOrderPreserved(ctx, received.snapshot());
	}

	void EventLoadTest::frameGatedCascade(partest::TestContext &ctx)
	{
		EventHarness harness(flavor(), "load-gated-casc-dispatcher");
		const cge::event::EventChannel<LoadPayload> &primary = harness.registry.getChannel<LoadPayload>("load-gated-casc-a");
		const cge::event::EventChannel<LoadPayload> &secondary = harness.registry.getChannel<LoadPayload>("load-gated-casc-b");
		PayloadLog sent;
		PayloadLog receivedPrimary;
		PayloadLog receivedSecondary;

		cge::event::BroadcasterBase broadcaster(&harness.dispatcher());
		cge::event::ListenerBase primaryListener(&harness.dispatcher());
		cge::event::ListenerBase secondaryListener(&harness.dispatcher());

		primaryListener.requestRegister(primary, [&](const LoadPayload &v) {
			receivedPrimary.record(v);
			// Cascade preserves the same payload identity on the secondary channel.
			broadcaster.broadcast(secondary, v);
		});
		secondaryListener.requestRegister(secondary, [&](const LoadPayload &v) {
			receivedSecondary.record(v);
		});
		harness.dispatcher().dispatchCommands();

		const unsigned workers = loadWorkerCount();
		const unsigned perWorker = pushesPerWorkerPerFrame / 2;

		const bool completed = runPersistentFrameGated("EventLoadTest.FrameGated.Cascade",
			workers, frameCount, perWorker,
			[&](unsigned frame, unsigned worker, unsigned seq) {
				const LoadPayload payload(frame, worker, seq);
				sent.record(payload);
				broadcaster.broadcast(primary, payload);
			},
			[&]() {
				harness.dispatcher().dispatchEvents();
			});
		ASSERT_TRUE(completed);
		if(!completed)
			return;

		// Trailing drains so the final frame's cascade is not left in the queue.
		for(unsigned extra = 0; extra < 8; ++extra)
			harness.dispatcher().dispatchEvents();

		std::vector<LoadPayload> expected = sent.snapshot();
		assertPayloadsPreserved(ctx, expected, receivedPrimary.snapshot());
		assertPayloadsPreserved(ctx, expected, receivedSecondary.snapshot());

		// The cascade re-broadcasts on delivery, so the secondary channel inherits
		// the primary's order. Per-producer order has to survive that hop.
		assertProducerOrderPreserved(ctx, receivedPrimary.snapshot());
		assertProducerOrderPreserved(ctx, receivedSecondary.snapshot());
	}

	// Events and commands in the same frame. Registration traffic is the only
	// command traffic a caller can legitimately produce, since ordinary channels
	// are refused at push, so this is what a mixed load actually looks like.
	//
	// Frame-gated and deterministic where concurrentChurn is free-running: every
	// command queued during a frame is drained in that frame, and a listener that
	// never churns must still receive every event regardless of what the
	// registration traffic is doing around it.
	void EventLoadTest::frameGatedChurn(partest::TestContext &ctx)
	{
		EventHarness harness(flavor(), "load-gated-churn-dispatcher");
		const cge::event::EventChannel<LoadPayload> &channel = harness.registry.getChannel<LoadPayload>("load-gated-churn");
		const cge::event::EventChannel<int> &churned = harness.registry.getChannel<int>("load-gated-churn-target");
		PayloadLog sent;
		PayloadLog received;

		cge::event::ListenerBase stable(&harness.dispatcher());
		stable.requestRegister(channel, [&received](const LoadPayload &v) { received.record(v); });
		harness.dispatcher().dispatchCommands();

		cge::event::BroadcasterBase broadcaster(&harness.dispatcher());
		const unsigned workers = loadWorkerCount();

		// One churn listener per worker. Listener-local state is not shared, so
		// giving each worker its own keeps the churn free of races between them
		// while still hammering the dispatcher's command path from all of them.
		std::vector<std::unique_ptr<cge::event::ListenerBase>> churn;
		churn.reserve(workers);
		for(unsigned w = 0; w < workers; ++w)
			churn.push_back(std::make_unique<cge::event::ListenerBase>(&harness.dispatcher()));

		const unsigned churnPushes = 64;

		const bool completed = runPersistentFrameGated("EventLoadTest.FrameGated.Churn",
			workers, frameCount, pushesPerWorkerPerFrame,
			[&](unsigned frame, unsigned worker, unsigned seq) {
				const LoadPayload payload(frame, worker, seq);
				sent.record(payload);
				broadcaster.broadcast(channel, payload);

				if(seq >= churnPushes)
					return;

				if(seq % 2 == 0)
					churn[worker]->requestRegister(churned, [](const int &) {});
				else
					churn[worker]->requestUnregister(churned);
			},
			[&]() {
				harness.dispatcher().dispatchCommands();
				harness.dispatcher().dispatchEvents();
			});
		ASSERT_TRUE(completed);
		if(!completed)
			return;

		assertPayloadsPreserved(ctx, sent.snapshot(), received.snapshot());
		assertProducerOrderPreserved(ctx, received.snapshot());
	}

	// --- Continuous: persistent workers fire the whole run; main steps frames ---

	void EventLoadTest::continuousWorkers(partest::TestContext &ctx)
	{
		EventHarness harness(flavor(), "load-cont-dispatcher");
		const cge::event::EventChannel<LoadPayload> &channel = harness.registry.getChannel<LoadPayload>("load-cont");
		PayloadLog sent;
		PayloadLog received;

		cge::event::ListenerBase listener(&harness.dispatcher());
		listener.requestRegister(channel, [&received](const LoadPayload &v) { received.record(v); });
		harness.dispatcher().dispatchCommands();

		cge::event::BroadcasterBase broadcaster(&harness.dispatcher());
		const unsigned workers = loadWorkerCount();
		const unsigned pushesPerWorker = pushesPerWorkerPerFrame * frameCount;

		std::vector<std::thread> threads;
		threads.reserve(workers);
		for(unsigned w = 0; w < workers; ++w)
		{
			threads.emplace_back([&broadcaster, &channel, &sent, w, pushesPerWorker]() {
				for(unsigned i = 0; i < pushesPerWorker; ++i)
				{
					// Frame slot folded into the high bits via i / perFrame for uniqueness.
					const unsigned frame = i / pushesPerWorkerPerFrame;
					const unsigned seq = i % pushesPerWorkerPerFrame;
					const LoadPayload payload(frame, w, seq);
					sent.record(payload);
					broadcaster.broadcast(channel, payload);
				}
			});
		}

		for(unsigned frame = 0; frame < frameCount; ++frame)
			harness.dispatcher().dispatchEvents();

		for(std::thread &t : threads)
			t.join();

		for(unsigned extra = 0; extra < 8; ++extra)
			harness.dispatcher().dispatchEvents();

		assertPayloadsPreserved(ctx, sent.snapshot(), received.snapshot());
		assertProducerOrderPreserved(ctx, received.snapshot());
	}

	void EventLoadTest::continuousCascade(partest::TestContext &ctx)
	{
		EventHarness harness(flavor(), "load-cont-casc-dispatcher");
		const cge::event::EventChannel<LoadPayload> &primary = harness.registry.getChannel<LoadPayload>("load-cont-casc-a");
		const cge::event::EventChannel<LoadPayload> &secondary = harness.registry.getChannel<LoadPayload>("load-cont-casc-b");
		PayloadLog sent;
		PayloadLog receivedPrimary;
		PayloadLog receivedSecondary;

		cge::event::BroadcasterBase broadcaster(&harness.dispatcher());
		cge::event::ListenerBase primaryListener(&harness.dispatcher());
		cge::event::ListenerBase secondaryListener(&harness.dispatcher());

		primaryListener.requestRegister(primary, [&](const LoadPayload &v) {
			receivedPrimary.record(v);
			broadcaster.broadcast(secondary, v);
		});
		secondaryListener.requestRegister(secondary, [&](const LoadPayload &v) {
			receivedSecondary.record(v);
		});
		harness.dispatcher().dispatchCommands();

		const unsigned workers = loadWorkerCount();
		const unsigned perFrame = pushesPerWorkerPerFrame / 2;
		const unsigned pushesPerWorker = perFrame * frameCount;

		std::vector<std::thread> threads;
		for(unsigned w = 0; w < workers; ++w)
		{
			threads.emplace_back([&broadcaster, &primary, &sent, w, pushesPerWorker, perFrame]() {
				for(unsigned i = 0; i < pushesPerWorker; ++i)
				{
					const unsigned frame = i / perFrame;
					const unsigned seq = i % perFrame;
					const LoadPayload payload(frame, w, seq);
					sent.record(payload);
					broadcaster.broadcast(primary, payload);
				}
			});
		}

		for(unsigned frame = 0; frame < frameCount; ++frame)
			harness.dispatcher().dispatchEvents();

		for(std::thread &t : threads)
			t.join();

		for(unsigned extra = 0; extra < 8; ++extra)
			harness.dispatcher().dispatchEvents();

		std::vector<LoadPayload> expected = sent.snapshot();
		assertPayloadsPreserved(ctx, expected, receivedPrimary.snapshot());
		assertPayloadsPreserved(ctx, expected, receivedSecondary.snapshot());

		// As in the frame-gated cascade, the secondary channel inherits the
		// primary's order, here with pushes arriving while the drain runs.
		assertProducerOrderPreserved(ctx, receivedPrimary.snapshot());
		assertProducerOrderPreserved(ctx, receivedSecondary.snapshot());
	}
}
