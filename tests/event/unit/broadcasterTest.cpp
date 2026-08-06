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

		addTest("BroadcastQueuesOnePush", flags, [this]() { broadcastQueuesOnePush(); });
		addTest("BroadcastUsesTheGivenChannel", flags, [this]() { broadcastUsesTheGivenChannel(); });
		addTest("BroadcastCarriesThePayload", flags, [this]() { broadcastCarriesThePayload(); });
		addTest("BroadcastCopiesThePayload", flags, [this]() { broadcastCopiesThePayload(); });
		addTest("BroadcastReturnsTrueWhenAccepted", flags, [this]() { broadcastReturnsTrueWhenAccepted(); });
		addTest("BroadcastReturnsFalseWhenRefused", flags, [this]() { broadcastReturnsFalseWhenRefused(); });
		addTest("BroadcastQueuesNothingWhenRefused", flags, [this]() { broadcastQueuesNothingWhenRefused(); });
		addTest("BroadcastLeavesCommandQueueEmpty", flags, [this]() { broadcastLeavesCommandQueueEmpty(); });

		addTest("CommandQueuesOnePush", flags, [this]() { commandQueuesOnePush(); });
		addTest("CommandUsesTheGivenChannel", flags, [this]() { commandUsesTheGivenChannel(); });
		addTest("CommandCarriesThePayload", flags, [this]() { commandCarriesThePayload(); });
		addTest("CommandReturnsTrueWhenAccepted", flags, [this]() { commandReturnsTrueWhenAccepted(); });
		addTest("CommandReturnsFalseWhenRefused", flags, [this]() { commandReturnsFalseWhenRefused(); });
		addTest("CommandQueuesNothingWhenRefused", flags, [this]() { commandQueuesNothingWhenRefused(); });
		addTest("CommandLeavesEventQueueEmpty", flags, [this]() { commandLeavesEventQueueEmpty(); });
	}

	void BroadcasterUnitTest::broadcastQueuesOnePush()
	{
		cge::event::EventChannelRegistry registry;
		MockDispatcher dispatcher(&registry);
		const cge::event::EventChannel<int> &channel = registry.getChannel<int>("bc");
		cge::event::BroadcasterBase broadcaster(&dispatcher);

		broadcaster.broadcast(channel, 1);

		ASSERT_EQUAL(dispatcher.eventCount(), static_cast<size_t>(1));
	}

	void BroadcasterUnitTest::broadcastUsesTheGivenChannel()
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

	void BroadcasterUnitTest::broadcastCarriesThePayload()
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
	void BroadcasterUnitTest::broadcastCopiesThePayload()
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

	void BroadcasterUnitTest::broadcastReturnsTrueWhenAccepted()
	{
		cge::event::EventChannelRegistry registry;
		MockDispatcher dispatcher(&registry);
		const cge::event::EventChannel<int> &channel = registry.getChannel<int>("bc");
		cge::event::BroadcasterBase broadcaster(&dispatcher);

		ASSERT_TRUE(broadcaster.broadcast(channel, 1));
	}

	void BroadcasterUnitTest::broadcastReturnsFalseWhenRefused()
	{
		cge::event::EventChannelRegistry registry;
		MockDispatcher dispatcher(&registry);
		const cge::event::EventChannel<int> &channel = registry.getChannel<int>("bc");
		cge::event::BroadcasterBase broadcaster(&dispatcher);
		dispatcher.setAcceptPushes(false);

		ASSERT_FALSE(broadcaster.broadcast(channel, 1));
	}

	void BroadcasterUnitTest::broadcastQueuesNothingWhenRefused()
	{
		cge::event::EventChannelRegistry registry;
		MockDispatcher dispatcher(&registry);
		const cge::event::EventChannel<int> &channel = registry.getChannel<int>("bc");
		cge::event::BroadcasterBase broadcaster(&dispatcher);
		dispatcher.setAcceptPushes(false);

		broadcaster.broadcast(channel, 1);

		ASSERT_EQUAL(dispatcher.eventCount(), static_cast<size_t>(0));
	}

	void BroadcasterUnitTest::broadcastLeavesCommandQueueEmpty()
	{
		cge::event::EventChannelRegistry registry;
		MockDispatcher dispatcher(&registry);
		const cge::event::EventChannel<int> &channel = registry.getChannel<int>("bc");
		cge::event::BroadcasterBase broadcaster(&dispatcher);

		broadcaster.broadcast(channel, 1);

		ASSERT_EQUAL(dispatcher.commandCount(), static_cast<size_t>(0));
	}

	void BroadcasterUnitTest::commandQueuesOnePush()
	{
		cge::event::EventChannelRegistry registry;
		MockDispatcher dispatcher(&registry);
		const cge::event::EventChannel<int> &channel = registry.getChannel<int>("cmd");
		cge::event::CommanderBase commander(&dispatcher);

		commander.command(channel, 1);

		ASSERT_EQUAL(dispatcher.commandCount(), static_cast<size_t>(1));
	}

	void BroadcasterUnitTest::commandUsesTheGivenChannel()
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

	void BroadcasterUnitTest::commandCarriesThePayload()
	{
		cge::event::EventChannelRegistry registry;
		MockDispatcher dispatcher(&registry);
		const cge::event::EventChannel<std::string> &channel = registry.getChannel<std::string>("cmd-str");
		cge::event::CommanderBase commander(&dispatcher);

		commander.command(channel, std::string("payload"));

		ASSERT_EQUAL(dispatcher.commandPayload<std::string>(0), std::string("payload"));
	}

	void BroadcasterUnitTest::commandReturnsTrueWhenAccepted()
	{
		cge::event::EventChannelRegistry registry;
		MockDispatcher dispatcher(&registry);
		const cge::event::EventChannel<int> &channel = registry.getChannel<int>("cmd");
		cge::event::CommanderBase commander(&dispatcher);

		ASSERT_TRUE(commander.command(channel, 1));
	}

	void BroadcasterUnitTest::commandReturnsFalseWhenRefused()
	{
		cge::event::EventChannelRegistry registry;
		MockDispatcher dispatcher(&registry);
		const cge::event::EventChannel<int> &channel = registry.getChannel<int>("cmd");
		cge::event::CommanderBase commander(&dispatcher);
		dispatcher.setAcceptPushes(false);

		ASSERT_FALSE(commander.command(channel, 1));
	}

	void BroadcasterUnitTest::commandQueuesNothingWhenRefused()
	{
		cge::event::EventChannelRegistry registry;
		MockDispatcher dispatcher(&registry);
		const cge::event::EventChannel<int> &channel = registry.getChannel<int>("cmd");
		cge::event::CommanderBase commander(&dispatcher);
		dispatcher.setAcceptPushes(false);

		commander.command(channel, 1);

		ASSERT_EQUAL(dispatcher.commandCount(), static_cast<size_t>(0));
	}

	void BroadcasterUnitTest::commandLeavesEventQueueEmpty()
	{
		cge::event::EventChannelRegistry registry;
		MockDispatcher dispatcher(&registry);
		const cge::event::EventChannel<int> &channel = registry.getChannel<int>("cmd");
		cge::event::CommanderBase commander(&dispatcher);

		commander.command(channel, 1);

		ASSERT_EQUAL(dispatcher.eventCount(), static_cast<size_t>(0));
	}
}
