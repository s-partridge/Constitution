#include "dispatcherLifecycleTest.h"

#include <memory>

#include <partest/assert.h>

#include "broadcaster.h"

namespace cge::test
{
	DispatcherLifecycleTest::DispatcherLifecycleTest(const DispatcherFlavor &flavor)
		: EventTestBase("DispatcherLifecycleTest", "Dispatcher setup, teardown, and intake refusal.", flavor)
	{
		partest::TestFlags flags = partest::TEST_FLAGS_INHERIT;

		addTest("Lifecycle", flags, [this]() { lifecycle(); });
		addTest("BroadcastPushResult", flags, [this]() { broadcastPushResult(); });
		addTest("CommandPushResult", flags, [this]() { commandPushResult(); });
	}

	// Each case needs a dispatcher at a different lifecycle point, so only the
	// registry and its channel identity are shared.
	void DispatcherLifecycleTest::lifecycle()
	{
		cge::event::EventChannelRegistry registry;
		const cge::event::EventChannel<int> &channel = registry.getChannel<int>("lifecycle");

		subtest("RegisterBeforeSetUp", [&]() {
			std::unique_ptr<cge::event::DispatcherBase> dispatcher = flavor().create("pre-setup", &registry);
			cge::event::ListenerBase listener(dispatcher.get());

			ASSERT_TRUE(listener.requestRegister(channel, [](const int &) {})
				== cge::event::RegistrationResult::Failure);
		});

		subtest("RegisterAfterTearDown", [&]() {
			std::unique_ptr<cge::event::DispatcherBase> dispatcher = flavor().create("post-teardown", &registry);
			dispatcher->setUp();
			dispatcher->tearDown();

			cge::event::ListenerBase listener(dispatcher.get());
			ASSERT_TRUE(listener.requestRegister(channel, [](const int &) {})
				== cge::event::RegistrationResult::Failure);

			cge::event::BroadcasterBase broadcaster(dispatcher.get());
			ASSERT_NOTHROW(broadcaster.broadcast(channel, 1));
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
		subtest("QueuedEventsDrainAfterTearDown", [&]() {
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
	void DispatcherLifecycleTest::broadcastPushResult()
	{
		cge::event::EventChannelRegistry registry;
		std::unique_ptr<cge::event::DispatcherBase> dispatcher = flavor().create("bc-result", &registry);
		const cge::event::EventChannel<int> &channel = registry.getChannel<int>("bc-result-ch");
		cge::event::BroadcasterBase broadcaster(dispatcher.get());

		subtest("FailsBeforeSetUp", [&]() {
			ASSERT_FALSE(broadcaster.broadcast(channel, 1));
		});

		subtest("SucceedsWhileActive", [&]() {
			dispatcher->setUp();
			ASSERT_TRUE(broadcaster.broadcast(channel, 2));
		});

		subtest("FailsAfterTearDown", [&]() {
			dispatcher->tearDown();
			ASSERT_FALSE(broadcaster.broadcast(channel, 3));
		});
	}

	// command reports whether the dispatcher accepted the push, same as broadcast.
	void DispatcherLifecycleTest::commandPushResult()
	{
		cge::event::EventChannelRegistry registry;
		std::unique_ptr<cge::event::DispatcherBase> dispatcher = flavor().create("cmd-result", &registry);
		const cge::event::EventChannel<int> &channel = registry.getChannel<int>("cmd-result-ch");
		cge::event::CommanderBase commander(dispatcher.get());

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
			dispatcher->setUp();
			dispatcher->tearDown();
			ASSERT_FALSE(commander.command(channel, 3));
		});
	}
}
