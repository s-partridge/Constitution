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

		addTest("Command", flags, PARTEST_CTX(this) { command(ctx); });
		addTest("NonRegistrationChannel", flags, PARTEST_CTX(this) { nonRegistrationChannel(ctx); });
	}

	void CommanderTest::command(partest::TestContext &ctx)
	{
		ctx.subtest("NotDelivered", PARTEST_CTX(&) {
			EventHarness harness(flavor(), "cmd-int-dispatcher");
			const cge::event::EventChannel<int> &channel = harness.registry.getChannel<int>("cmd-int");
			CountingListener listener(&harness.dispatcher());

			listener.requestRegister(channel, [&listener](const int &v) { listener.onInt(v); });
			harness.dispatcher().dispatchCommands();

			cge::event::CommanderBase commander(&harness.dispatcher());
			commander.command(channel, 55);
			harness.dispatcher().dispatchCommands();
			harness.dispatcher().dispatchEvents();

			ASSERT_EQUAL(listener.received.size(), 0u);
		});

		// The command never reaches a queue at all, so the command drain has
		// nothing to do and only the broadcast survives to the event drain.
		ctx.subtest("NoCrossover", PARTEST_CTX(&) {
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

			ASSERT_EQUAL(listener.received.size(), 1u);
			ASSERT_EQUAL(listener.received[0], 2);
		});

	}

	// Ensure invalid channels are rejects as commands.
	void CommanderTest::nonRegistrationChannel(partest::TestContext &ctx)
	{
		ctx.subtest("Rejected", PARTEST_CTX(&) {
			EventHarness harness(flavor(), "cmd-only-dispatcher");
			const cge::event::EventChannel<int> &channel = harness.registry.getChannel<int>("cmd-only");
			CountingListener listener(&harness.dispatcher());

			listener.requestRegister(channel, [&listener](const int &v) { listener.onInt(v); });
			harness.dispatcher().dispatchCommands();

			cge::event::CommanderBase commander(&harness.dispatcher());
			ASSERT_EQUAL(commander.command(channel, 99), event::DispatchStatus::Invalid);

			harness.dispatcher().dispatchCommands();
			harness.dispatcher().dispatchEvents();
			ASSERT_EQUAL(listener.received.size(), 0u);
		});

		// TODO: bool collapses "the dispatcher is inactive" and "this channel is
		// not valid for commands" into one false. They are different conditions
		// for the caller: the first means retry later, the second means the
		// calling code is wrong. The value assertions elsewhere stay correct
		// under either signature, so the coarseness is only visible on the type.
		// decltype leaves the call unevaluated, so nothing is pushed here.
		ctx.subtest("ResultType", "Ensure Commander.command() does not return boolean type", PARTEST_CTX(&) {
			EventHarness harness(flavor(), "cmd-result-type-dispatcher");
			const cge::event::EventChannel<int> &channel = harness.registry.getChannel<int>("cmd-result-type");
			cge::event::CommanderBase commander(&harness.dispatcher());

			const bool returnsBool =
				std::is_same<decltype(commander.command(channel, 99)), bool>::value;

			ASSERT_FALSE(returnsBool);
		});
	}
}
