#include "dispatcherLifecycleTest.h"

#include <memory>

#include <partest/assert.h>

#include "broadcaster.h"

namespace cge::test
{
	namespace
	{
		// TODO: the enum has no value for "the dispatcher is not accepting work".
		// Failure is documented as an unknown error, which is the one thing this
		// is not: a system registering during construction has to tell "retry
		// after setUp" apart from "something is broken". Assertions against this
		// clear when Rejected or BadState is added to RegistrationResult.
		//
		// The value sits outside the current enumerator range so it cannot match
		// by accident. Its numeric position is arbitrary and will not survive an
		// enumerator being inserted rather than appended, which is part of what
		// the TODO is here to catch.
		const cge::event::RegistrationResult kRejectedPlaceholder =
			static_cast<cge::event::RegistrationResult>(100);

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
		: EventTestBase("DispatcherLifecycleTest", "Dispatcher setup, teardown, and intake refusal.", flavor)
	{
		partest::TestFlags flags = partest::TEST_FLAGS_INHERIT;

		addTest("Lifecycle", flags, [this]() { lifecycle(); });
		addTest("Restoration", flags, [this]() { restoration(); });
		addTest("Destruction", flags, [this]() { destruction(); });
		addTest("BroadcastPushResult", flags, [this]() { broadcastPushResult(); });
		addTest("CommandPushResult", flags, [this]() { commandPushResult(); });
	}

	// Each case needs a dispatcher at a different lifecycle point, so only the
	// registry and its channel identity are shared.
	void DispatcherLifecycleTest::lifecycle()
	{
		cge::event::EventChannelRegistry registry;
		const cge::event::EventChannel<int> &channel = registry.getChannel<int>("lifecycle");

		// The negative states the contract in readable form. On its own it would
		// go green the moment the call started returning Success or Duplicate,
		// which are equally wrong, so the placeholder comparison carries the
		// actual requirement.
		subtest("RegisterBeforeSetUp", [&]() {
			std::unique_ptr<cge::event::DispatcherBase> dispatcher = flavor().create("pre-setup", &registry);
			cge::event::ListenerBase listener(dispatcher.get());

			const cge::event::RegistrationResult result =
				listener.requestRegister(channel, [](const int &) {});

			ASSERT_FALSE(result == cge::event::RegistrationResult::Failure);
			ASSERT_TRUE(result == kRejectedPlaceholder);
		});

		subtest("RegisterAfterTearDown", [&]() {
			std::unique_ptr<cge::event::DispatcherBase> dispatcher = flavor().create("post-teardown", &registry);
			dispatcher->setUp();
			dispatcher->tearDown();

			cge::event::ListenerBase listener(dispatcher.get());
			const cge::event::RegistrationResult result =
				listener.requestRegister(channel, [](const int &) {});

			ASSERT_FALSE(result == cge::event::RegistrationResult::Failure);
			ASSERT_TRUE(result == kRejectedPlaceholder);
		});

		// Unregistration stays valid while inactive: a listener must always be
		// able to leave, whatever state the dispatcher is in.
		subtest("UnregisterStaysValid", [&]() {
			std::unique_ptr<cge::event::DispatcherBase> dispatcher = flavor().create("unreg-inactive", &registry);
			dispatcher->setUp();

			CountingListener listener(dispatcher.get());
			listener.requestRegister(channel, [&listener](const int &v) { listener.onInt(v); });
			dispatcher->dispatchCommands();
			dispatcher->tearDown();

			const cge::event::RegistrationResult result = listener.requestUnregister(channel);
			ASSERT_FALSE(result == cge::event::RegistrationResult::Failure);
			ASSERT_FALSE(result == kRejectedPlaceholder);
		});

		// Dispatch always drains, so a parked event would surface on the next
		// drain. Nothing arriving therefore proves the push was refused outright.
		subtest("InactivePushDiscarded", [&]() {
			std::unique_ptr<cge::event::DispatcherBase> dispatcher = flavor().create("inactive-drop", &registry);
			CountingListener listener(dispatcher.get());
			cge::event::BroadcasterBase broadcaster(dispatcher.get());

			broadcaster.broadcast(channel, 1);

			dispatcher->setUp();
			listener.requestRegister(channel, [&listener](const int &v) { listener.onInt(v); });
			dispatcher->dispatchCommands();
			dispatcher->dispatchEvents();
			ASSERT_EQUAL(listener.received.size(), static_cast<size_t>(0));

			dispatcher->tearDown();
			broadcaster.broadcast(channel, 2);
			dispatcher->dispatchEvents();
			ASSERT_EQUAL(listener.received.size(), static_cast<size_t>(0));
		});

		// tearDown stops intake, not processing: the engine keeps driving dispatch
		// on its own schedule, and work already queued still drains.
		subtest("DrainAfterTearDown", [&]() {
			std::unique_ptr<cge::event::DispatcherBase> dispatcher = flavor().create("drain-after-teardown", &registry);
			dispatcher->setUp();

			CountingListener listener(dispatcher.get());
			listener.requestRegister(channel, [&listener](const int &v) { listener.onInt(v); });
			dispatcher->dispatchCommands();

			cge::event::BroadcasterBase broadcaster(dispatcher.get());
			broadcaster.broadcast(channel, 5);
			dispatcher->tearDown();

			dispatcher->dispatchEvents();
			ASSERT_EQUAL(listener.received.size(), static_cast<size_t>(1));
			ASSERT_EQUAL(listener.received[0], 5);
		});
	}

	// broadcast reports whether the dispatcher accepted the push. A refused push
	// is discarded, so the caller's return value is the only signal it gets.
	// The ordinary level transition: tear the dispatcher down, bring it back, and
	// keep running. Nothing here is an edge case; it is what happens between any
	// two levels.
	void DispatcherLifecycleTest::restoration()
	{
		cge::event::EventChannelRegistry registry;
		const cge::event::EventChannel<int> &channel = registry.getChannel<int>("restore");

		// The listener is registered up front so the only thing varying across the
		// teardown is whether a push is taken at all.
		subtest("SetUpRestoresIntake", [&]() {
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
			ASSERT_EQUAL(listener.received.size(), static_cast<size_t>(1));
			if(listener.received.size() == 1)
				ASSERT_EQUAL(listener.received[0], 2);

			dispatcher->tearDown();
		});

		// Once accepted, always delivered. tearDown refuses new intake and never
		// discards what is already queued, so an event that was accepted before
		// the level ended is still there when the next one starts.
		subtest("QueuedWorkSurvives", [&]() {
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

			ASSERT_EQUAL(listener.received.size(), static_cast<size_t>(1));
			if(listener.received.size() == 1)
				ASSERT_EQUAL(listener.received[0], 5);

			dispatcher->tearDown();
		});

		// Listeners are never evicted from the map, so a registration made before
		// the teardown is still live after the next setUp. Revisit when eviction
		// lands: this is the case an unconditional flush would change.
		subtest("RegistrationsSurvive", [&]() {
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

			ASSERT_EQUAL(listener.received.size(), static_cast<size_t>(1));
			if(listener.received.size() == 1)
				ASSERT_EQUAL(listener.received[0], 9);

			dispatcher->tearDown();
		});
	}

	// The dispatcher outlives its listeners by contract, so the ordering is a
	// precondition rather than a case. What is left to prove is that going away
	// with work still queued releases that work instead of leaking it.
	void DispatcherLifecycleTest::destruction()
	{
		cge::event::EventChannelRegistry registry;
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
	}

	// Each case builds its own dispatcher at the lifecycle point it needs, so a
	// failure in one does not change what the next one is testing.
	void DispatcherLifecycleTest::broadcastPushResult()
	{
		cge::event::EventChannelRegistry registry;
		const cge::event::EventChannel<int> &channel = registry.getChannel<int>("bc-result-ch");

		subtest("FailsBeforeSetUp", [&]() {
			std::unique_ptr<cge::event::DispatcherBase> dispatcher = flavor().create("bc-pre", &registry);
			cge::event::BroadcasterBase broadcaster(dispatcher.get());

			ASSERT_FALSE(broadcaster.broadcast(channel, 1));
		});

		subtest("SucceedsWhileActive", [&]() {
			std::unique_ptr<cge::event::DispatcherBase> dispatcher = flavor().create("bc-active", &registry);
			dispatcher->setUp();
			cge::event::BroadcasterBase broadcaster(dispatcher.get());

			ASSERT_TRUE(broadcaster.broadcast(channel, 2));
		});

		subtest("FailsAfterTearDown", [&]() {
			std::unique_ptr<cge::event::DispatcherBase> dispatcher = flavor().create("bc-post", &registry);
			dispatcher->setUp();
			dispatcher->tearDown();
			cge::event::BroadcasterBase broadcaster(dispatcher.get());

			ASSERT_FALSE(broadcaster.broadcast(channel, 3));
		});
	}

	// command reports whether the dispatcher accepted the push, same as broadcast.
	void DispatcherLifecycleTest::commandPushResult()
	{
		cge::event::EventChannelRegistry registry;
		const cge::event::EventChannel<int> &channel = registry.getChannel<int>("cmd-result-ch");

		subtest("FailsBeforeSetUp", [&]() {
			std::unique_ptr<cge::event::DispatcherBase> dispatcher = flavor().create("cmd-pre", &registry);
			cge::event::CommanderBase commander(dispatcher.get());

			ASSERT_FALSE(commander.command(channel, 1));
		});

		// TODO: no valid command exists to push. Channels are validated at push
		// time and the only ones accepted carry registration or unregistration,
		// which come from ListenerBase and never through CommanderBase. Fill this
		// in when a command vocabulary exists that a caller can legitimately send.
		subtest("SucceedsWhileActive", partest::TEST_FLAGS_SKIP, [&]() {
		});

		subtest("FailsAfterTearDown", [&]() {
			std::unique_ptr<cge::event::DispatcherBase> dispatcher = flavor().create("cmd-post", &registry);
			dispatcher->setUp();
			dispatcher->tearDown();
			cge::event::CommanderBase commander(dispatcher.get());

			ASSERT_FALSE(commander.command(channel, 3));
		});
	}
}
