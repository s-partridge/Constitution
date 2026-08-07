#include "asyncDispatcherTest.h"

#include <memory>
#include <string>

#include <partest/assert.h>

#include "asyncDispatcher.h"
#include "event.h"

namespace cge::test
{
	namespace
	{
		// Republishes what the dispatcher already keeps protected, so a test can
		// push directly and read the queues back without a collaborator.
		class TestableAsyncDispatcher : public cge::event::AsyncDispatcher
		{
		public:
			TestableAsyncDispatcher(const std::string &name, cge::event::EventChannelRegistry *registry)
				: AsyncDispatcher(name, registry)
			{
			}

			using AsyncDispatcher::onPushEvent;
			using AsyncDispatcher::onPushCommand;

			size_t queuedEvents() const { return m_events.size(); }
			size_t queuedCommands() const { return m_commands.size(); }
			bool active() const { return m_active; }
		};

		std::unique_ptr<cge::event::EventBase> makeEvent(int value)
		{
			return std::make_unique<cge::event::Event<int>>(value);
		}
	}

	AsyncDispatcherUnitTest::AsyncDispatcherUnitTest()
		: TestBase("AsyncDispatcherUnitTest", "Unit tests for AsyncDispatcher lifecycle and queues.")
	{
		partest::TestFlags flags = partest::TEST_FLAGS_INHERIT;

		addTest("StartsInactive", flags, [this]() { startsInactive(); });
		addTest("SetUpActivates", flags, [this]() { setUpActivates(); });
		addTest("TearDownDeactivates", flags, [this]() { tearDownDeactivates(); });
		addTest("Reactivates", flags, [this]() { reactivates(); });
		addTest("RepeatedSetUp", flags, [this]() { repeatedSetUp(); });
		addTest("RepeatedTearDown", flags, [this]() { repeatedTearDown(); });

		addTest("InactiveRefused", flags, [this]() { inactiveRefused(); });
		addTest("InactiveQueuesNothing", flags, [this]() { inactiveQueuesNothing(); });
		addTest("EventQueued", flags, [this]() { eventQueued(); });
		addTest("CommandQueued", flags, [this]() { commandQueued(); });
		addTest("EventNotInCommands", flags, [this]() { eventNotInCommands(); });
		addTest("CommandNotInEvents", flags, [this]() { commandNotInEvents(); });

		addTest("DrainEmpty", flags, [this]() { drainEmpty(); });
		addTest("DrainNoListeners", flags, [this]() { drainNoListeners(); });
		addTest("QueueSurvivesTearDown", flags, [this]() { queueSurvivesTearDown(); });
	}

	void AsyncDispatcherUnitTest::startsInactive()
	{
		cge::event::EventChannelRegistry registry;
		TestableAsyncDispatcher dispatcher("new", &registry);

		ASSERT_FALSE(dispatcher.active());
	}

	void AsyncDispatcherUnitTest::setUpActivates()
	{
		cge::event::EventChannelRegistry registry;
		TestableAsyncDispatcher dispatcher("setup", &registry);

		dispatcher.setUp();

		ASSERT_TRUE(dispatcher.active());
	}

	void AsyncDispatcherUnitTest::tearDownDeactivates()
	{
		cge::event::EventChannelRegistry registry;
		TestableAsyncDispatcher dispatcher("teardown", &registry);

		dispatcher.setUp();
		dispatcher.tearDown();

		ASSERT_FALSE(dispatcher.active());
	}

	// The level transition path: a dispatcher torn down between levels has to
	// come back when the next one starts.
	void AsyncDispatcherUnitTest::reactivates()
	{
		cge::event::EventChannelRegistry registry;
		TestableAsyncDispatcher dispatcher("recycle", &registry);
		const cge::event::EventChannel<int> &channel = registry.getChannel<int>("recycle-ch");

		dispatcher.setUp();
		dispatcher.tearDown();
		dispatcher.setUp();

		ASSERT_TRUE(dispatcher.active());
		ASSERT_TRUE(dispatcher.onPushEvent(channel, makeEvent(1)));
	}

	void AsyncDispatcherUnitTest::repeatedSetUp()
	{
		cge::event::EventChannelRegistry registry;
		TestableAsyncDispatcher dispatcher("double-setup", &registry);
		const cge::event::EventChannel<int> &channel = registry.getChannel<int>("double-setup-ch");

		dispatcher.setUp();
		dispatcher.setUp();

		ASSERT_TRUE(dispatcher.active());
		ASSERT_TRUE(dispatcher.onPushEvent(channel, makeEvent(1)));
		ASSERT_EQUAL(dispatcher.queuedEvents(), static_cast<size_t>(1));
	}

	void AsyncDispatcherUnitTest::repeatedTearDown()
	{
		cge::event::EventChannelRegistry registry;
		TestableAsyncDispatcher dispatcher("double-teardown", &registry);

		dispatcher.setUp();
		dispatcher.tearDown();
		dispatcher.tearDown();

		ASSERT_FALSE(dispatcher.active());
	}

	void AsyncDispatcherUnitTest::inactiveRefused()
	{
		cge::event::EventChannelRegistry registry;
		TestableAsyncDispatcher dispatcher("inactive", &registry);
		const cge::event::EventChannel<int> &channel = registry.getChannel<int>("inactive-ch");

		ASSERT_FALSE(dispatcher.onPushEvent(channel, makeEvent(1)));
		ASSERT_FALSE(dispatcher.onPushCommand(channel, makeEvent(1)));
	}

	void AsyncDispatcherUnitTest::inactiveQueuesNothing()
	{
		cge::event::EventChannelRegistry registry;
		TestableAsyncDispatcher dispatcher("inactive-queue", &registry);
		const cge::event::EventChannel<int> &channel = registry.getChannel<int>("inactive-queue-ch");

		dispatcher.onPushEvent(channel, makeEvent(1));
		dispatcher.onPushCommand(channel, makeEvent(1));

		ASSERT_EQUAL(dispatcher.queuedEvents(), static_cast<size_t>(0));
		ASSERT_EQUAL(dispatcher.queuedCommands(), static_cast<size_t>(0));
	}

	void AsyncDispatcherUnitTest::eventQueued()
	{
		cge::event::EventChannelRegistry registry;
		TestableAsyncDispatcher dispatcher("event-push", &registry);
		const cge::event::EventChannel<int> &channel = registry.getChannel<int>("event-push-ch");
		dispatcher.setUp();

		ASSERT_TRUE(dispatcher.onPushEvent(channel, makeEvent(1)));
		ASSERT_EQUAL(dispatcher.queuedEvents(), static_cast<size_t>(1));
	}

	void AsyncDispatcherUnitTest::commandQueued()
	{
		cge::event::EventChannelRegistry registry;
		TestableAsyncDispatcher dispatcher("command-push", &registry);
		const cge::event::EventChannel<int> &channel = registry.getChannel<int>("command-push-ch");
		dispatcher.setUp();

		ASSERT_TRUE(dispatcher.onPushCommand(channel, makeEvent(1)));
		ASSERT_EQUAL(dispatcher.queuedCommands(), static_cast<size_t>(1));
	}

	void AsyncDispatcherUnitTest::eventNotInCommands()
	{
		cge::event::EventChannelRegistry registry;
		TestableAsyncDispatcher dispatcher("event-only", &registry);
		const cge::event::EventChannel<int> &channel = registry.getChannel<int>("event-only-ch");
		dispatcher.setUp();

		dispatcher.onPushEvent(channel, makeEvent(1));

		ASSERT_EQUAL(dispatcher.queuedCommands(), static_cast<size_t>(0));
	}

	void AsyncDispatcherUnitTest::commandNotInEvents()
	{
		cge::event::EventChannelRegistry registry;
		TestableAsyncDispatcher dispatcher("command-only", &registry);
		const cge::event::EventChannel<int> &channel = registry.getChannel<int>("command-only-ch");
		dispatcher.setUp();

		dispatcher.onPushCommand(channel, makeEvent(1));

		ASSERT_EQUAL(dispatcher.queuedEvents(), static_cast<size_t>(0));
	}

	void AsyncDispatcherUnitTest::drainEmpty()
	{
		cge::event::EventChannelRegistry registry;
		TestableAsyncDispatcher dispatcher("empty-drain", &registry);
		dispatcher.setUp();

		dispatcher.dispatchCommands();
		dispatcher.dispatchEvents();

		ASSERT_EQUAL(dispatcher.queuedEvents(), static_cast<size_t>(0));
		ASSERT_EQUAL(dispatcher.queuedCommands(), static_cast<size_t>(0));
	}

	// Nobody is registered, so there is nowhere for the event to go. It still
	// has to leave the queue rather than accumulating there.
	void AsyncDispatcherUnitTest::drainNoListeners()
	{
		cge::event::EventChannelRegistry registry;
		TestableAsyncDispatcher dispatcher("no-listeners", &registry);
		const cge::event::EventChannel<int> &channel = registry.getChannel<int>("no-listeners-ch");
		dispatcher.setUp();

		dispatcher.onPushEvent(channel, makeEvent(1));
		ASSERT_EQUAL(dispatcher.queuedEvents(), static_cast<size_t>(1));

		dispatcher.dispatchEvents();

		ASSERT_EQUAL(dispatcher.queuedEvents(), static_cast<size_t>(0));
	}

	// Once accepted, always delivered, at its smallest testable size: tearDown
	// stops intake but must not discard what is already queued.
	void AsyncDispatcherUnitTest::queueSurvivesTearDown()
	{
		cge::event::EventChannelRegistry registry;
		TestableAsyncDispatcher dispatcher("survive", &registry);
		const cge::event::EventChannel<int> &channel = registry.getChannel<int>("survive-ch");
		dispatcher.setUp();

		dispatcher.onPushEvent(channel, makeEvent(1));
		dispatcher.tearDown();

		ASSERT_EQUAL(dispatcher.queuedEvents(), static_cast<size_t>(1));

		dispatcher.dispatchEvents();

		ASSERT_EQUAL(dispatcher.queuedEvents(), static_cast<size_t>(0));
	}
}
