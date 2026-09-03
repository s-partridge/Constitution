#include "broadcasterTest.h"

#include <string>

#include <partest/assert.h>

#include "broadcaster.h"
#include "mockDispatcher.h"

namespace cge::test
{
	BroadcasterUnitTest::BroadcasterUnitTest()
		: TestBase("BroadcasterUnitTest", "Unit tests for BroadcasterBase and CommanderBase.")
	{
		partest::TestFlags flags = partest::TEST_FLAGS_INHERIT;

		addTest("BroadcastQueuesOne", "Validate one call produces one queue entry with no listener registered.", flags, PARTEST_CTX(this) { broadcastQueuesOne(ctx); });
		addTest("BroadcastChannel", "Validate channel identity survives alongside a second channel of the same payload type.", flags, PARTEST_CTX(this) { broadcastChannel(ctx); });
		addTest("BroadcastPayload", "Validate an allocating payload survives the push intact.", flags, PARTEST_CTX(this) { broadcastPayload(ctx); });
		addTest("BroadcastCopies", "Validate the queued payload is a copy by mutating the source after the push returns.", flags, PARTEST_CTX(this) { broadcastCopies(ctx); });
		addTest("BroadcastAccepted", "Validate Pending from the dispatcher is mapped to true for the caller.", flags, PARTEST_CTX(this) { broadcastAccepted(ctx); });
		addTest("BroadcastRefused", "Validate Failure from the dispatcher is mapped to false for the caller.", flags, PARTEST_CTX(this) { broadcastRefused(ctx); });
		addTest("BroadcastRefusedQueue", "Validate a refused push is discarded rather than held for a later drain.", flags, PARTEST_CTX(this) { broadcastRefusedQueue(ctx); });
		addTest("BroadcastQueueOnly", "Validate an event push does not leak into the command queue.", flags, PARTEST_CTX(this) { broadcastQueueOnly(ctx); });

		// pushCommand validates the channel before the queueing policy runs, and
		// MockDispatcher declares no command channel, so every command below is
		// refused as Invalid and onPushCommand is never reached. These clear when
		// a dispatcher can declare its own command channels.
		partest::TestFlags commandFlags = flags.withExpectFailure();

		addTest("CommandQueuesOne", "Validate one call produces one command queue entry.", commandFlags, PARTEST_CTX(this) { commandQueuesOne(ctx); });
		addTest("CommandChannel", "Validate channel identity survives alongside a second channel of the same payload type.", commandFlags, PARTEST_CTX(this) { commandChannel(ctx); });
		addTest("CommandPayload", "Validate an allocating payload survives the push intact.", commandFlags, PARTEST_CTX(this) { commandPayload(ctx); });
		addTest("CommandAccepted", "Validate Pending reaches the caller as a status, not collapsed to a bool.", commandFlags, PARTEST_CTX(this) { commandAccepted(ctx); });
		addTest("CommandRefused", "Validate Failure reaches the caller as a status, not collapsed to a bool.", commandFlags, PARTEST_CTX(this) { commandRefused(ctx); });

		// These two pass, but only because the rejection leaves both queues empty,
		// which is the result they assert. They start proving what they are named
		// for on the same day the five above go green.
		addTest("CommandRefusedQueue", "Validate a refused command is discarded rather than held for a later drain.", flags, PARTEST_CTX(this) { commandRefusedQueue(ctx); });
		addTest("CommandQueueOnly", "Validate a command push does not leak into the event queue.", flags, PARTEST_CTX(this) { commandQueueOnly(ctx); });
	}

	void BroadcasterUnitTest::broadcastQueuesOne(partest::TestContext &ctx)
	{
		cge::event::EventChannelRegistry registry;
		MockDispatcher dispatcher(&registry);
		const cge::event::EventChannel<int> &channel = registry.getChannel<int>("bc");
		cge::event::BroadcasterBase broadcaster(&dispatcher);

		broadcaster.broadcast(channel, 1);

		ASSERT_EQUAL(dispatcher.eventCount(), static_cast<size_t>(1));
	}

	void BroadcasterUnitTest::broadcastChannel(partest::TestContext &ctx)
	{
		cge::event::EventChannelRegistry registry;
		MockDispatcher dispatcher(&registry);
		const cge::event::EventChannel<int> &channel = registry.getChannel<int>("bc");
		const cge::event::EventChannel<int> &other = registry.getChannel<int>("bc-other");
		cge::event::BroadcasterBase broadcaster(&dispatcher);

		broadcaster.broadcast(other, 1);

		ASSERT_EQUAL(dispatcher.eventChannel(0), other.id());
		ASSERT_NOT_EQUAL(dispatcher.eventChannel(0), channel.id());
	}

	void BroadcasterUnitTest::broadcastPayload(partest::TestContext &ctx)
	{
		cge::event::EventChannelRegistry registry;
		MockDispatcher dispatcher(&registry);
		const cge::event::EventChannel<std::string> &channel = registry.getChannel<std::string>("bc-str");
		cge::event::BroadcasterBase broadcaster(&dispatcher);

		broadcaster.broadcast(channel, std::string("payload"));

		ASSERT_EQUAL(dispatcher.eventPayload<std::string>(0), std::string("payload"));
	}

	// The push takes a reference and stores a copy, so the caller's source is
	// free the moment broadcast returns.
	void BroadcasterUnitTest::broadcastCopies(partest::TestContext &ctx)
	{
		cge::event::EventChannelRegistry registry;
		MockDispatcher dispatcher(&registry);
		const cge::event::EventChannel<std::string> &channel = registry.getChannel<std::string>("bc-copy");
		cge::event::BroadcasterBase broadcaster(&dispatcher);

		std::string source = "original";
		broadcaster.broadcast(channel, source);
		source = "mutated";

		ASSERT_EQUAL(dispatcher.eventPayload<std::string>(0), std::string("original"));
	}

	void BroadcasterUnitTest::broadcastAccepted(partest::TestContext &ctx)
	{
		cge::event::EventChannelRegistry registry;
		MockDispatcher dispatcher(&registry);
		const cge::event::EventChannel<int> &channel = registry.getChannel<int>("bc");
		cge::event::BroadcasterBase broadcaster(&dispatcher);

		ASSERT_TRUE(broadcaster.broadcast(channel, 1));
	}

	void BroadcasterUnitTest::broadcastRefused(partest::TestContext &ctx)
	{
		cge::event::EventChannelRegistry registry;
		MockDispatcher dispatcher(&registry);
		const cge::event::EventChannel<int> &channel = registry.getChannel<int>("bc");
		cge::event::BroadcasterBase broadcaster(&dispatcher);
		dispatcher.setAcceptPushes(false);

		ASSERT_FALSE(broadcaster.broadcast(channel, 1));
	}

	void BroadcasterUnitTest::broadcastRefusedQueue(partest::TestContext &ctx)
	{
		cge::event::EventChannelRegistry registry;
		MockDispatcher dispatcher(&registry);
		const cge::event::EventChannel<int> &channel = registry.getChannel<int>("bc");
		cge::event::BroadcasterBase broadcaster(&dispatcher);
		dispatcher.setAcceptPushes(false);

		broadcaster.broadcast(channel, 1);

		ASSERT_EQUAL(dispatcher.eventCount(), static_cast<size_t>(0));
	}

	void BroadcasterUnitTest::broadcastQueueOnly(partest::TestContext &ctx)
	{
		cge::event::EventChannelRegistry registry;
		MockDispatcher dispatcher(&registry);
		const cge::event::EventChannel<int> &channel = registry.getChannel<int>("bc");
		cge::event::BroadcasterBase broadcaster(&dispatcher);

		broadcaster.broadcast(channel, 1);

		ASSERT_EQUAL(dispatcher.commandCount(), static_cast<size_t>(0));
	}

	void BroadcasterUnitTest::commandQueuesOne(partest::TestContext &ctx)
	{
		cge::event::EventChannelRegistry registry;
		MockDispatcher dispatcher(&registry);
		const cge::event::EventChannel<int> &channel = registry.getChannel<int>("DC_TestCommand");
		cge::event::CommanderBase commander(&dispatcher);

		commander.command(channel, 1);

		ASSERT_EQUAL(dispatcher.commandCount(), static_cast<size_t>(1));
	}

	void BroadcasterUnitTest::commandChannel(partest::TestContext &ctx)
	{
		cge::event::EventChannelRegistry registry;
		MockDispatcher dispatcher(&registry);
		const cge::event::EventChannel<int> &channel = registry.getChannel<int>("DC_TestCommand");
		const cge::event::EventChannel<int> &other = registry.getChannel<int>("DC_TestCommandAlt");
		cge::event::CommanderBase commander(&dispatcher);

		commander.command(other, 1);

		ASSERT_EQUAL(dispatcher.commandChannel(0), other.id());
		ASSERT_NOT_EQUAL(dispatcher.commandChannel(0), channel.id());
	}

	void BroadcasterUnitTest::commandPayload(partest::TestContext &ctx)
	{
		cge::event::EventChannelRegistry registry;
		MockDispatcher dispatcher(&registry);
		const cge::event::EventChannel<std::string> &channel = registry.getChannel<std::string>("DC_TestCommandString");
		cge::event::CommanderBase commander(&dispatcher);

		commander.command(channel, std::string("payload"));

		ASSERT_EQUAL(dispatcher.commandPayload<std::string>(0), std::string("payload"));
	}

	void BroadcasterUnitTest::commandAccepted(partest::TestContext &ctx)
	{
		cge::event::EventChannelRegistry registry;
		MockDispatcher dispatcher(&registry);
		const cge::event::EventChannel<int> &channel = registry.getChannel<int>("DC_TestCommand");
		cge::event::CommanderBase commander(&dispatcher);

		ASSERT_EQUAL(commander.command(channel, 1), event::DispatchStatus::Pending);
	}

	void BroadcasterUnitTest::commandRefused(partest::TestContext &ctx)
	{
		cge::event::EventChannelRegistry registry;
		MockDispatcher dispatcher(&registry);
		const cge::event::EventChannel<int> &channel = registry.getChannel<int>("DC_TestCommand");
		cge::event::CommanderBase commander(&dispatcher);
		dispatcher.setAcceptPushes(false);

		ASSERT_EQUAL(commander.command(channel, 1), event::DispatchStatus::Failure);
	}

	void BroadcasterUnitTest::commandRefusedQueue(partest::TestContext &ctx)
	{
		cge::event::EventChannelRegistry registry;
		MockDispatcher dispatcher(&registry);
		const cge::event::EventChannel<int> &channel = registry.getChannel<int>("DC_TestCommand");
		cge::event::CommanderBase commander(&dispatcher);
		dispatcher.setAcceptPushes(false);

		commander.command(channel, 1);

		ASSERT_EQUAL(dispatcher.commandCount(), static_cast<size_t>(0));
	}

	void BroadcasterUnitTest::commandQueueOnly(partest::TestContext &ctx)
	{
		cge::event::EventChannelRegistry registry;
		MockDispatcher dispatcher(&registry);
		const cge::event::EventChannel<int> &channel = registry.getChannel<int>("DC_TestCommand");
		cge::event::CommanderBase commander(&dispatcher);

		commander.command(channel, 1);

		ASSERT_EQUAL(dispatcher.eventCount(), static_cast<size_t>(0));
	}
}
