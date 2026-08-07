#include "commanderTest.h"

#include <memory>
#include <type_traits>

#include <partest/assert.h>

#include "broadcaster.h"

namespace cge::test
{
	CommanderTest::CommanderTest(const DispatcherFlavor &flavor)
		: DispatcherFlavorSuite("CommanderTest", "Command enqueue and channel validation.", flavor)
	{
		partest::TestFlags flags = partest::TEST_FLAGS_INHERIT;

		addTest("Command", flags, [this]() { command(); });
		addTest("NonRegistrationChannel", flags, [this]() { nonRegistrationChannel(); });
	}

	void CommanderTest::command()
	{
		subtest("NotDelivered", [&]() {
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

		// The command never reaches a queue at all, so the command drain has
		// nothing to do and only the broadcast survives to the event drain.
		subtest("NoCrossover", [&]() {
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

		// TODO: bool collapses "the dispatcher is inactive" and "this channel is
		// not valid for commands" into one false. They are different conditions
		// for the caller: the first means retry later, the second means the
		// calling code is wrong. The value assertions elsewhere stay correct
		// under either signature, so the coarseness is only visible on the type.
		// decltype leaves the call unevaluated, so nothing is pushed here.
		subtest("ResultType", [&]() {
			EventHarness harness(flavor(), "cmd-result-type-dispatcher");
			const cge::event::EventChannel<int> &channel = harness.registry.getChannel<int>("cmd-result-type");
			cge::event::CommanderBase commander(&harness.dispatcher());

			const bool returnsBool =
				std::is_same<decltype(commander.command(channel, 99)), bool>::value;

			ASSERT_FALSE(returnsBool);
		});
	}
}
