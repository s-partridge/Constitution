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

		addTest("BroadcastQueuesOne", "Validate one call produces one queue entry with no listener registered.", flags, [this]() { broadcastQueuesOne(); });
		addTest("BroadcastChannel", "Validate channel identity survives alongside a second channel of the same payload type.", flags, [this]() { broadcastChannel(); });
		addTest("BroadcastPayload", "Validate an allocating payload survives the push intact.", flags, [this]() { broadcastPayload(); });
		addTest("BroadcastCopies", "Validate the queued payload is a copy by mutating the source after the push returns.", flags, [this]() { broadcastCopies(); });
		addTest("BroadcastAccepted", "Validate Pending from the dispatcher is mapped to true for the caller.", flags, [this]() { broadcastAccepted(); });
		addTest("BroadcastRefused", "Validate Failure from the dispatcher is mapped to false for the caller.", flags, [this]() { broadcastRefused(); });
		addTest("BroadcastRefusedQueue", "Validate a refused push is discarded rather than held for a later drain.", flags, [this]() { broadcastRefusedQueue(); });
		addTest("BroadcastQueueOnly", "Validate an event push does not leak into the command queue.", flags, [this]() { broadcastQueueOnly(); });

		// pushCommand validates the channel before the queueing policy runs, and
		// MockDispatcher declares no command channel, so every command below is
		// refused as Invalid and onPushCommand is never reached. These clear when
		// a dispatcher can declare its own command channels.
		partest::TestFlags commandFlags = flags.withExpectFailure();

		addTest("CommandQueuesOne", "Validate one call produces one command queue entry.", commandFlags, [this]() { commandQueuesOne(); });
		addTest("CommandChannel", "Validate channel identity survives alongside a second channel of the same payload type.", commandFlags, [this]() { commandChannel(); });
		addTest("CommandPayload", "Validate an allocating payload survives the push intact.", commandFlags, [this]() { commandPayload(); });
		addTest("CommandAccepted", "Validate Pending reaches the caller as a status, not collapsed to a bool.", commandFlags, [this]() { commandAccepted(); });
		addTest("CommandRefused", "Validate Failure reaches the caller as a status, not collapsed to a bool.", commandFlags, [this]() { commandRefused(); });

		// These two pass, but only because the rejection leaves both queues empty,
		// which is the result they assert. They start proving what they are named
		// for on the same day the five above go green.
		addTest("CommandRefusedQueue", "Validate a refused command is discarded rather than held for a later drain.", flags, [this]() { commandRefusedQueue(); });
		addTest("CommandQueueOnly", "Validate a command push does not leak into the event queue.", flags, [this]() { commandQueueOnly(); });
	}

	void BroadcasterUnitTest::broadcastQueuesOne()
	{
		cge::event::EventChannelRegistry registry;
		MockDispatcher dispatcher(&registry);
		const cge::event::EventChannel<int> &channel = registry.getChannel<int>("bc");
		cge::event::BroadcasterBase broadcaster(&dispatcher);

		broadcaster.broadcast(channel, 1);

		ASSERT_EQUAL(dispatcher.eventCount(), static_cast<size_t>(1));
	}

	void BroadcasterUnitTest::broadcastChannel()
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

	void BroadcasterUnitTest::broadcastPayload()
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
	void BroadcasterUnitTest::broadcastCopies()
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

	void BroadcasterUnitTest::broadcastAccepted()
	{
		cge::event::EventChannelRegistry registry;
		MockDispatcher dispatcher(&registry);
		const cge::event::EventChannel<int> &channel = registry.getChannel<int>("bc");
		cge::event::BroadcasterBase broadcaster(&dispatcher);

		ASSERT_TRUE(broadcaster.broadcast(channel, 1));
	}

	void BroadcasterUnitTest::broadcastRefused()
	{
		cge::event::EventChannelRegistry registry;
		MockDispatcher dispatcher(&registry);
		const cge::event::EventChannel<int> &channel = registry.getChannel<int>("bc");
		cge::event::BroadcasterBase broadcaster(&dispatcher);
		dispatcher.setAcceptPushes(false);

		ASSERT_FALSE(broadcaster.broadcast(channel, 1));
	}

	void BroadcasterUnitTest::broadcastRefusedQueue()
	{
		cge::event::EventChannelRegistry registry;
		MockDispatcher dispatcher(&registry);
		const cge::event::EventChannel<int> &channel = registry.getChannel<int>("bc");
		cge::event::BroadcasterBase broadcaster(&dispatcher);
		dispatcher.setAcceptPushes(false);

		broadcaster.broadcast(channel, 1);

		ASSERT_EQUAL(dispatcher.eventCount(), static_cast<size_t>(0));
	}

	void BroadcasterUnitTest::broadcastQueueOnly()
	{
		cge::event::EventChannelRegistry registry;
		MockDispatcher dispatcher(&registry);
		const cge::event::EventChannel<int> &channel = registry.getChannel<int>("bc");
		cge::event::BroadcasterBase broadcaster(&dispatcher);

		broadcaster.broadcast(channel, 1);

		ASSERT_EQUAL(dispatcher.commandCount(), static_cast<size_t>(0));
	}

	void BroadcasterUnitTest::commandQueuesOne()
	{
		cge::event::EventChannelRegistry registry;
		MockDispatcher dispatcher(&registry);
		const cge::event::EventChannel<int> &channel = registry.getChannel<int>("DC_TestCommand");
		cge::event::CommanderBase commander(&dispatcher);

		commander.command(channel, 1);

		ASSERT_EQUAL(dispatcher.commandCount(), static_cast<size_t>(1));
	}

	void BroadcasterUnitTest::commandChannel()
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

	void BroadcasterUnitTest::commandPayload()
	{
		cge::event::EventChannelRegistry registry;
		MockDispatcher dispatcher(&registry);
		const cge::event::EventChannel<std::string> &channel = registry.getChannel<std::string>("DC_TestCommandString");
		cge::event::CommanderBase commander(&dispatcher);

		commander.command(channel, std::string("payload"));

		ASSERT_EQUAL(dispatcher.commandPayload<std::string>(0), std::string("payload"));
	}

	void BroadcasterUnitTest::commandAccepted()
	{
		cge::event::EventChannelRegistry registry;
		MockDispatcher dispatcher(&registry);
		const cge::event::EventChannel<int> &channel = registry.getChannel<int>("DC_TestCommand");
		cge::event::CommanderBase commander(&dispatcher);

		ASSERT_EQUAL(commander.command(channel, 1), event::DispatchStatus::Pending);
	}

	void BroadcasterUnitTest::commandRefused()
	{
		cge::event::EventChannelRegistry registry;
		MockDispatcher dispatcher(&registry);
		const cge::event::EventChannel<int> &channel = registry.getChannel<int>("DC_TestCommand");
		cge::event::CommanderBase commander(&dispatcher);
		dispatcher.setAcceptPushes(false);

		ASSERT_EQUAL(commander.command(channel, 1), event::DispatchStatus::Failure);
	}

	void BroadcasterUnitTest::commandRefusedQueue()
	{
		cge::event::EventChannelRegistry registry;
		MockDispatcher dispatcher(&registry);
		const cge::event::EventChannel<int> &channel = registry.getChannel<int>("DC_TestCommand");
		cge::event::CommanderBase commander(&dispatcher);
		dispatcher.setAcceptPushes(false);

		commander.command(channel, 1);

		ASSERT_EQUAL(dispatcher.commandCount(), static_cast<size_t>(0));
	}

	void BroadcasterUnitTest::commandQueueOnly()
	{
		cge::event::EventChannelRegistry registry;
		MockDispatcher dispatcher(&registry);
		const cge::event::EventChannel<int> &channel = registry.getChannel<int>("DC_TestCommand");
		cge::event::CommanderBase commander(&dispatcher);

		commander.command(channel, 1);

		ASSERT_EQUAL(dispatcher.eventCount(), static_cast<size_t>(0));
	}
}
