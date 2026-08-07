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

		addTest("BroadcastQueuesOne", flags, [this]() { broadcastQueuesOne(); });
		addTest("BroadcastChannel", flags, [this]() { broadcastChannel(); });
		addTest("BroadcastPayload", flags, [this]() { broadcastPayload(); });
		addTest("BroadcastCopies", flags, [this]() { broadcastCopies(); });
		addTest("BroadcastAccepted", flags, [this]() { broadcastAccepted(); });
		addTest("BroadcastRefused", flags, [this]() { broadcastRefused(); });
		addTest("BroadcastRefusedQueue", flags, [this]() { broadcastRefusedQueue(); });
		addTest("BroadcastQueueOnly", flags, [this]() { broadcastQueueOnly(); });

		addTest("CommandQueuesOne", flags, [this]() { commandQueuesOne(); });
		addTest("CommandChannel", flags, [this]() { commandChannel(); });
		addTest("CommandPayload", flags, [this]() { commandPayload(); });
		addTest("CommandAccepted", flags, [this]() { commandAccepted(); });
		addTest("CommandRefused", flags, [this]() { commandRefused(); });
		addTest("CommandRefusedQueue", flags, [this]() { commandRefusedQueue(); });
		addTest("CommandQueueOnly", flags, [this]() { commandQueueOnly(); });
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
		const cge::event::EventChannel<int> &channel = registry.getChannel<int>("cmd");
		cge::event::CommanderBase commander(&dispatcher);

		commander.command(channel, 1);

		ASSERT_EQUAL(dispatcher.commandCount(), static_cast<size_t>(1));
	}

	void BroadcasterUnitTest::commandChannel()
	{
		cge::event::EventChannelRegistry registry;
		MockDispatcher dispatcher(&registry);
		const cge::event::EventChannel<int> &channel = registry.getChannel<int>("cmd");
		const cge::event::EventChannel<int> &other = registry.getChannel<int>("cmd-other");
		cge::event::CommanderBase commander(&dispatcher);

		commander.command(other, 1);

		ASSERT_EQUAL(dispatcher.commandChannel(0), other.id());
		ASSERT_NOT_EQUAL(dispatcher.commandChannel(0), channel.id());
	}

	void BroadcasterUnitTest::commandPayload()
	{
		cge::event::EventChannelRegistry registry;
		MockDispatcher dispatcher(&registry);
		const cge::event::EventChannel<std::string> &channel = registry.getChannel<std::string>("cmd-str");
		cge::event::CommanderBase commander(&dispatcher);

		commander.command(channel, std::string("payload"));

		ASSERT_EQUAL(dispatcher.commandPayload<std::string>(0), std::string("payload"));
	}

	void BroadcasterUnitTest::commandAccepted()
	{
		cge::event::EventChannelRegistry registry;
		MockDispatcher dispatcher(&registry);
		const cge::event::EventChannel<int> &channel = registry.getChannel<int>("cmd");
		cge::event::CommanderBase commander(&dispatcher);

		ASSERT_TRUE(commander.command(channel, 1));
	}

	void BroadcasterUnitTest::commandRefused()
	{
		cge::event::EventChannelRegistry registry;
		MockDispatcher dispatcher(&registry);
		const cge::event::EventChannel<int> &channel = registry.getChannel<int>("cmd");
		cge::event::CommanderBase commander(&dispatcher);
		dispatcher.setAcceptPushes(false);

		ASSERT_FALSE(commander.command(channel, 1));
	}

	void BroadcasterUnitTest::commandRefusedQueue()
	{
		cge::event::EventChannelRegistry registry;
		MockDispatcher dispatcher(&registry);
		const cge::event::EventChannel<int> &channel = registry.getChannel<int>("cmd");
		cge::event::CommanderBase commander(&dispatcher);
		dispatcher.setAcceptPushes(false);

		commander.command(channel, 1);

		ASSERT_EQUAL(dispatcher.commandCount(), static_cast<size_t>(0));
	}

	void BroadcasterUnitTest::commandQueueOnly()
	{
		cge::event::EventChannelRegistry registry;
		MockDispatcher dispatcher(&registry);
		const cge::event::EventChannel<int> &channel = registry.getChannel<int>("cmd");
		cge::event::CommanderBase commander(&dispatcher);

		commander.command(channel, 1);

		ASSERT_EQUAL(dispatcher.eventCount(), static_cast<size_t>(0));
	}
}
