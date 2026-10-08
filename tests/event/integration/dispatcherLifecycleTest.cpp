#include "dispatcherLifecycleTest.h"

#include <memory>

#include <partest/assert.h>

#include "broadcaster.h"

namespace cge::test
{
	namespace
	{
		// Counts its own live instances, so a queue torn down with events still
		// in it can be shown to have released them rather than leaked them.
		struct TrackedPayload
		{
			static int live;

			int value;

			explicit TrackedPayload(int value)
				: value(value)
			{
				++live;
			}

			TrackedPayload(const TrackedPayload &other)
				: value(other.value)
			{
				++live;
			}

			TrackedPayload &operator=(const TrackedPayload &other)
			{
				value = other.value;
				return *this;
			}

			~TrackedPayload()
			{
				--live;
			}
		};

		int TrackedPayload::live = 0;
	}

	DispatcherLifecycleTest::DispatcherLifecycleTest(const DispatcherFlavor &flavor)
		: DispatcherFlavorSuite("DispatcherLifecycleTest", "Dispatcher setup, teardown, and intake refusal.", flavor)
	{
		partest::TestFlags flags = partest::TEST_FLAGS_INHERIT;

		addTest("Lifecycle", flags, PARTEST_CTX(this) { lifecycle(ctx); });
		addTest("Restoration", flags, PARTEST_CTX(this) { restoration(ctx); });
		addTest("Destruction", flags, PARTEST_CTX(this) { destruction(ctx); });
		addTest("BroadcastPushResult", flags, PARTEST_CTX(this) { broadcastPushResult(ctx); });
		addTest("CommandPushResult", flags, PARTEST_CTX(this) { commandPushResult(ctx); });
	}

	// Each case needs a dispatcher at a different lifecycle point, so only the
	// registry and its channel identity are shared.
	void DispatcherLifecycleTest::lifecycle(partest::TestContext &ctx)
	{
		cge::event::EventChannelRegistry registry;
		const cge::event::EventChannel<int> &channel = registry.getChannel<int>("lifecycle");

		// The negative states the contract in readable form. On its own it would
		// go green the moment the call started returning Success or Duplicate,
		// which are equally wrong, so the placeholder comparison carries the
		// actual requirement.
		ctx.subtest("RegisterBeforeSetUp", PARTEST_CTX(&) {
			std::unique_ptr<cge::event::DispatcherBase> dispatcher = flavor().create("pre-setup", &registry);
			cge::event::ListenerBase listener(dispatcher.get());

			const cge::event::DispatchStatus result =
				listener.requestRegister(channel, [](const int &) {});

			ASSERT_EQUAL(result, cge::event::DispatchStatus::NotReady);
		});

		ctx.subtest("RegisterAfterTearDown", PARTEST_CTX(&) {
			std::unique_ptr<cge::event::DispatcherBase> dispatcher = flavor().create("post-teardown", &registry);
			dispatcher->setUp();
			dispatcher->tearDown();

			cge::event::ListenerBase listener(dispatcher.get());
			const cge::event::DispatchStatus result =
				listener.requestRegister(channel, [](const int &) {});

			ASSERT_EQUAL(result, cge::event::DispatchStatus::NotReady);
		});

		// Unregistration stays valid while inactive: a listener must always be
		// able to leave, whatever state the dispatcher is in.
		ctx.subtest("UnregisterStaysValid", PARTEST_CTX(&) {
			std::unique_ptr<cge::event::DispatcherBase> dispatcher = flavor().create("unreg-inactive", &registry);
			dispatcher->setUp();

			CountingListener listener(dispatcher.get());
			listener.requestRegister(channel, [&listener](const int &v) { listener.onInt(v); });
			dispatcher->dispatchCommands();
			dispatcher->tearDown();

			const cge::event::DispatchStatus result = listener.requestUnregister(channel);
			ASSERT_EQUAL(result, cge::event::DispatchStatus::Pending);

			// Accepted while inactive means the command still applies. A component
			// may finish tearing down before the next level starts, so resuming the
			// dispatcher must not revive the listener.
			dispatcher->dispatchCommands();
			dispatcher->setUp();

			cge::event::BroadcasterBase broadcaster(dispatcher.get());
			ASSERT_TRUE(broadcaster.broadcast(channel, 1));
			dispatcher->dispatchEvents();
			ASSERT_EQUAL(listener.received.size(), 0u);

			dispatcher->tearDown();
		});

		// Refusal is not a deferred registration. Once the next level begins the
		// listener may submit a new callback, and only that callback may become
		// live.
		ctx.subtest("RefusedRegistrationLeavesNoState", PARTEST_CTX(&) {
			std::unique_ptr<cge::event::DispatcherBase> dispatcher = flavor().create("refused-register", &registry);
			int refusedCalls = 0;
			int acceptedCalls = 0;
			cge::event::ListenerBase listener(dispatcher.get());

			ASSERT_EQUAL(listener.requestRegister(channel, [&refusedCalls](const int &) {
				++refusedCalls;
			}), cge::event::DispatchStatus::NotReady);

			dispatcher->setUp();
			const cge::event::DispatchStatus retry = listener.requestRegister(channel, [&acceptedCalls](const int &) {
				++acceptedCalls;
			});
			ASSERT_EQUAL(retry, cge::event::DispatchStatus::Pending);
			if(retry != cge::event::DispatchStatus::Pending)
			{
				dispatcher->tearDown();
				return;
			}
			dispatcher->dispatchCommands();

			cge::event::BroadcasterBase broadcaster(dispatcher.get());
			ASSERT_TRUE(broadcaster.broadcast(channel, 1));
			dispatcher->dispatchEvents();

			ASSERT_EQUAL(refusedCalls, 0);
			ASSERT_EQUAL(acceptedCalls, 1);
			dispatcher->tearDown();
		});

		// Dispatch always drains, so a parked event would surface on the next
		// drain. Nothing arriving therefore proves the push was refused outright.
		ctx.subtest("InactivePushDiscarded", PARTEST_CTX(&) {
			std::unique_ptr<cge::event::DispatcherBase> dispatcher = flavor().create("inactive-drop", &registry);
			CountingListener listener(dispatcher.get());
			cge::event::BroadcasterBase broadcaster(dispatcher.get());

			broadcaster.broadcast(channel, 1);

			dispatcher->setUp();
			listener.requestRegister(channel, [&listener](const int &v) { listener.onInt(v); });
			dispatcher->dispatchCommands();
			dispatcher->dispatchEvents();
			ASSERT_EQUAL(listener.received.size(), 0u);

			dispatcher->tearDown();
			broadcaster.broadcast(channel, 2);
			dispatcher->dispatchEvents();
			ASSERT_EQUAL(listener.received.size(), 0u);
		});

		// tearDown stops intake, not processing: the engine keeps driving dispatch
		// on its own schedule, and work already queued still drains.
		ctx.subtest("DrainAfterTearDown", PARTEST_CTX(&) {
			std::unique_ptr<cge::event::DispatcherBase> dispatcher = flavor().create("drain-after-teardown", &registry);
			dispatcher->setUp();

			CountingListener listener(dispatcher.get());
			listener.requestRegister(channel, [&listener](const int &v) { listener.onInt(v); });
			dispatcher->dispatchCommands();

			cge::event::BroadcasterBase broadcaster(dispatcher.get());
			broadcaster.broadcast(channel, 5);
			dispatcher->tearDown();

			dispatcher->dispatchEvents();
			ASSERT_EQUAL(listener.received.size(), 1u);
			ASSERT_EQUAL(listener.received[0], 5);
		});
	}

	// broadcast reports whether the dispatcher accepted the push. A refused push
	// is discarded, so the caller's return value is the only signal it gets.
	// The ordinary level transition: tear the dispatcher down, bring it back, and
	// keep running. Nothing here is an edge case; it is what happens between any
	// two levels.
	void DispatcherLifecycleTest::restoration(partest::TestContext &ctx)
	{
		cge::event::EventChannelRegistry registry;
		const cge::event::EventChannel<int> &channel = registry.getChannel<int>("restore");

		// The listener is registered up front so the only thing varying across the
		// teardown is whether a push is taken at all.
		ctx.subtest("SetUpRestoresIntake", PARTEST_CTX(&) {
			std::unique_ptr<cge::event::DispatcherBase> dispatcher = flavor().create("restore-intake", &registry);
			dispatcher->setUp();

			CountingListener listener(dispatcher.get());
			listener.requestRegister(channel, [&listener](const int &v) { listener.onInt(v); });
			dispatcher->dispatchCommands();

			cge::event::BroadcasterBase broadcaster(dispatcher.get());
			dispatcher->tearDown();
			ASSERT_FALSE(broadcaster.broadcast(channel, 1));

			dispatcher->setUp();
			ASSERT_TRUE(broadcaster.broadcast(channel, 2));
			dispatcher->dispatchEvents();

			// The refused push left nothing queued behind it, so the drain that
			// delivers 2 is the proof that 1 never entered.
			ASSERT_EQUAL(listener.received.size(), 1u);
			if(listener.received.size() == 1)
				ASSERT_EQUAL(listener.received[0], 2);

			dispatcher->tearDown();
		});

		// Once accepted, always delivered. tearDown refuses new intake and never
		// discards what is already queued, so an event that was accepted before
		// the level ended is still there when the next one starts.
		ctx.subtest("QueuedWorkSurvives", PARTEST_CTX(&) {
			std::unique_ptr<cge::event::DispatcherBase> dispatcher = flavor().create("restore-queue", &registry);
			dispatcher->setUp();

			CountingListener listener(dispatcher.get());
			listener.requestRegister(channel, [&listener](const int &v) { listener.onInt(v); });
			dispatcher->dispatchCommands();

			cge::event::BroadcasterBase broadcaster(dispatcher.get());
			broadcaster.broadcast(channel, 5);

			dispatcher->tearDown();
			dispatcher->setUp();
			dispatcher->dispatchEvents();

			ASSERT_EQUAL(listener.received.size(), 1u);
			if(listener.received.size() == 1)
				ASSERT_EQUAL(listener.received[0], 5);

			dispatcher->tearDown();
		});

		// Teardown does not remove listeners, so a registration made before the
		// teardown is still live after the next setUp.
		ctx.subtest("RegistrationsSurvive", PARTEST_CTX(&) {
			std::unique_ptr<cge::event::DispatcherBase> dispatcher = flavor().create("restore-reg", &registry);
			dispatcher->setUp();

			CountingListener listener(dispatcher.get());
			listener.requestRegister(channel, [&listener](const int &v) { listener.onInt(v); });
			dispatcher->dispatchCommands();

			dispatcher->tearDown();
			dispatcher->setUp();

			cge::event::BroadcasterBase broadcaster(dispatcher.get());
			broadcaster.broadcast(channel, 9);
			dispatcher->dispatchEvents();

			ASSERT_EQUAL(listener.received.size(), 1u);
			if(listener.received.size() == 1)
				ASSERT_EQUAL(listener.received[0], 9);

			dispatcher->tearDown();
		});
	}

	// The dispatcher outlives its listeners by contract, so the ordering is a
	// precondition rather than a case. What is left to prove is that going away
	// with work still queued releases that work instead of leaking it.
	void DispatcherLifecycleTest::destruction(partest::TestContext &ctx)
	{
		cge::event::EventChannelRegistry registry;

		ctx.subtest("QueuedEvents", PARTEST_CTX(&) {
			const cge::event::EventChannel<TrackedPayload> &channel =
				registry.getChannel<TrackedPayload>("destroy-queued");

			TrackedPayload::live = 0;
			{
				std::unique_ptr<cge::event::DispatcherBase> dispatcher = flavor().create("destroy-queued", &registry);
				dispatcher->setUp();

				// The listener goes first, as the lifetime law requires.
				{
					cge::event::ListenerBase listener(dispatcher.get());
					listener.requestRegister(channel, [](const TrackedPayload &) {});
					dispatcher->dispatchCommands();

					cge::event::BroadcasterBase broadcaster(dispatcher.get());
					for(int value = 0; value < 4; ++value)
						broadcaster.broadcast(channel, TrackedPayload(value));
				}

				// Four events accepted and never drained.
				ASSERT_EQUAL(TrackedPayload::live, 4);
			}

			ASSERT_EQUAL(TrackedPayload::live, 0);
		});

		// The command queue owns its entries the same way the event queue does.
		// Registration requests are the only commands that can be queued today and
		// their payload type is private, so the only countable command is one on a
		// channel of the test's own. Expected to fail: no dispatcher can declare a
		// command channel yet, so the command is refused as Invalid and nothing is
		// queued to count. Once declaration exists, declare this channel on the
		// dispatcher before commanding. See docs/expected-failures.md.
		ctx.subtest("QueuedCommands", partest::TEST_FLAGS_INHERIT.withExpectFailure(), PARTEST_CTX(&) {
			const cge::event::EventChannel<TrackedPayload> &channel =
				registry.getChannel<TrackedPayload>("destroy-queued-commands");

			TrackedPayload::live = 0;
			{
				std::unique_ptr<cge::event::DispatcherBase> dispatcher = flavor().create("destroy-queued-commands", &registry);
				dispatcher->setUp();

				cge::event::CommanderBase commander(dispatcher.get());
				for(int value = 0; value < 4; ++value)
					commander.command(channel, TrackedPayload(value));

				// Four commands accepted and never drained.
				ASSERT_EQUAL(TrackedPayload::live, 4);
			}

			ASSERT_EQUAL(TrackedPayload::live, 0);
		});
	}

	// Each case builds its own dispatcher at the lifecycle point it needs, so a
	// failure in one does not change what the next one is testing.
	void DispatcherLifecycleTest::broadcastPushResult(partest::TestContext &ctx)
	{
		cge::event::EventChannelRegistry registry;
		const cge::event::EventChannel<int> &channel = registry.getChannel<int>("bc-result-ch");

		ctx.subtest("FailsBeforeSetUp", PARTEST_CTX(&) {
			std::unique_ptr<cge::event::DispatcherBase> dispatcher = flavor().create("bc-pre", &registry);
			cge::event::BroadcasterBase broadcaster(dispatcher.get());

			ASSERT_FALSE(broadcaster.broadcast(channel, 1));
		});

		ctx.subtest("SucceedsWhileActive", PARTEST_CTX(&) {
			std::unique_ptr<cge::event::DispatcherBase> dispatcher = flavor().create("bc-active", &registry);
			dispatcher->setUp();
			cge::event::BroadcasterBase broadcaster(dispatcher.get());

			ASSERT_TRUE(broadcaster.broadcast(channel, 2));
		});

		ctx.subtest("FailsAfterTearDown", PARTEST_CTX(&) {
			std::unique_ptr<cge::event::DispatcherBase> dispatcher = flavor().create("bc-post", &registry);
			dispatcher->setUp();
			dispatcher->tearDown();
			cge::event::BroadcasterBase broadcaster(dispatcher.get());

			ASSERT_FALSE(broadcaster.broadcast(channel, 3));
		});
	}

	// command reports whether the dispatcher accepted the push, same as broadcast.
	//
	// Both refusal cases are marked expectFailure rather than skipped, because the
	// contract is settled even though it cannot be met yet: a command that the
	// dispatcher is not ready to take reports NotReady. The channel here is an
	// ordinary one and so is not a valid command channel, which pushCommand
	// rejects as Invalid before readiness is ever consulted, so both report
	// Invalid today. They go green when a dispatcher can declare its own command
	// channels and this one can be declared on it, at which point the assertion
	// is about readiness alone. See docs/expected-failures.md.
	void DispatcherLifecycleTest::commandPushResult(partest::TestContext &ctx)
	{
		cge::event::EventChannelRegistry registry;
		const cge::event::EventChannel<int> &channel = registry.getChannel<int>("cmd-result-ch");

		ctx.subtest("FailsBeforeSetUp", partest::TEST_FLAGS_INHERIT.withExpectFailure(), PARTEST_CTX(&) {
			std::unique_ptr<cge::event::DispatcherBase> dispatcher = flavor().create("cmd-pre", &registry);
			cge::event::CommanderBase commander(dispatcher.get());

			ASSERT_EQUAL(commander.command(channel, 1), event::DispatchStatus::NotReady);
		});

		// TODO: no valid command exists to push. Channels are validated at push
		// time and the only ones accepted carry registration or unregistration,
		// which come from ListenerBase and never through CommanderBase. Fill this
		// in when a command vocabulary exists that a caller can legitimately send.
		ctx.subtest("SucceedsWhileActive", partest::TEST_FLAGS_SKIP, PARTEST_CTX(&) {
		});

		ctx.subtest("FailsAfterTearDown", partest::TEST_FLAGS_INHERIT.withExpectFailure(), PARTEST_CTX(&) {
			std::unique_ptr<cge::event::DispatcherBase> dispatcher = flavor().create("cmd-post", &registry);
			dispatcher->setUp();
			dispatcher->tearDown();
			cge::event::CommanderBase commander(dispatcher.get());

			ASSERT_EQUAL(commander.command(channel, 1), event::DispatchStatus::NotReady);
		});
	}
}
