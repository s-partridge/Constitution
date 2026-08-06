#include "commanderTest.h"

#include <memory>

#include <partest/assert.h>

#include "broadcaster.h"

namespace cge::test
{
	CommanderTest::CommanderTest(const DispatcherFlavor &flavor)
		: EventTestBase("CommanderTest", "Command enqueue and channel validation.", flavor)
	{
		partest::TestFlags flags = partest::TEST_FLAGS_INHERIT;

		addTest("Command", flags, [this]() { command(); });
		addTest("NonRegistrationChannel", flags, [this]() { nonRegistrationChannel(); });
	}

	void CommanderTest::command()
	{
		subtest("NotDeliveredToListeners", [&]() {
			EventHarness harness(flavor(), "cmd-int-dispatcher");
			const cge::event::EventChannel<int> &channel = harness.registry.getChannel<int>("cmd-int");
			CountingListener listener(&harness.dispatcher());

			listener.requestRegister(channel, [&listener](const int &v) { listener.onInt(v); });
			harness.dispatcher().dispatchCommands();

			cge::event::CommanderBase commander(&harness.dispatcher());
			commander.command(channel, 55);
			harness.dispatcher().dispatchCommands();
			harness.dispatcher().dispatchEvents();

			ASSERT_EQUAL(listener.received.size(), static_cast<size_t>(0));
		});

		// Drain commands first (drops the command payload), then events.
		subtest("NoCrossoverToEvents", [&]() {
			EventHarness harness(flavor(), "cmd-vs-evt-dispatcher");
			const cge::event::EventChannel<int> &channel = harness.registry.getChannel<int>("cmd-vs-evt");
			CountingListener listener(&harness.dispatcher());

			listener.requestRegister(channel, [&listener](const int &v) { listener.onInt(v); });
			harness.dispatcher().dispatchCommands();

			cge::event::CommanderBase commander(&harness.dispatcher());
			cge::event::BroadcasterBase broadcaster(&harness.dispatcher());

			commander.command(channel, 1);
			broadcaster.broadcast(channel, 2);
			harness.dispatcher().dispatchCommands();
			harness.dispatcher().dispatchEvents();

			ASSERT_EQUAL(listener.received.size(), static_cast<size_t>(1));
			ASSERT_EQUAL(listener.received[0], 2);
		});

		subtest("AfterTearDown", [&]() {
			cge::event::EventChannelRegistry registry;
			std::unique_ptr<cge::event::DispatcherBase> dispatcher = flavor().create("cmd-td", &registry);
			dispatcher->setUp();
			const cge::event::EventChannel<int> &channel = registry.getChannel<int>("cmd-td-ch");
			cge::event::CommanderBase commander(dispatcher.get());
			dispatcher->tearDown();

			ASSERT_NOTHROW(commander.command(channel, 1));
		});
	}

	// A command addressed to an ordinary channel is refused: the dispatcher only
	// accepts registration and unregistration, and the caller is told rather
	// than having the command silently discarded at the drain.
	void CommanderTest::nonRegistrationChannel()
	{
		subtest("Rejected", [&]() {
			EventHarness harness(flavor(), "cmd-only-dispatcher");
			const cge::event::EventChannel<int> &channel = harness.registry.getChannel<int>("cmd-only");
			CountingListener listener(&harness.dispatcher());

			listener.requestRegister(channel, [&listener](const int &v) { listener.onInt(v); });
			harness.dispatcher().dispatchCommands();

			cge::event::CommanderBase commander(&harness.dispatcher());
			ASSERT_FALSE(commander.command(channel, 99));

			harness.dispatcher().dispatchCommands();
			harness.dispatcher().dispatchEvents();
			ASSERT_EQUAL(listener.received.size(), static_cast<size_t>(0));
		});
	}
}
