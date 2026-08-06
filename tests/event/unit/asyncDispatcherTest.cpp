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

		addTest("NewDispatcherIsInactive", flags, [this]() { newDispatcherIsInactive(); });
		addTest("SetUpActivates", flags, [this]() { setUpActivates(); });
		addTest("TearDownDeactivates", flags, [this]() { tearDownDeactivates(); });
		addTest("SetUpAfterTearDownReactivates", flags, [this]() { setUpAfterTearDownReactivates(); });
		addTest("RepeatedSetUpIsIdempotent", flags, [this]() { repeatedSetUpIsIdempotent(); });
		addTest("RepeatedTearDownIsIdempotent", flags, [this]() { repeatedTearDownIsIdempotent(); });

		addTest("PushWhileInactiveIsRefused", flags, [this]() { pushWhileInactiveIsRefused(); });
		addTest("PushWhileInactiveQueuesNothing", flags, [this]() { pushWhileInactiveQueuesNothing(); });
		addTest("EventPushLandsInEventQueue", flags, [this]() { eventPushLandsInEventQueue(); });
		addTest("CommandPushLandsInCommandQueue", flags, [this]() { commandPushLandsInCommandQueue(); });
		addTest("EventPushLeavesCommandQueueEmpty", flags, [this]() { eventPushLeavesCommandQueueEmpty(); });
		addTest("CommandPushLeavesEventQueueEmpty", flags, [this]() { commandPushLeavesEventQueueEmpty(); });

		addTest("DispatchOnEmptyQueuesIsSafe", flags, [this]() { dispatchOnEmptyQueuesIsSafe(); });
		addTest("DispatchEventsDrainsWithNoListeners", flags, [this]() { dispatchEventsDrainsWithNoListeners(); });
		addTest("QueuedEventsSurviveTearDown", flags, [this]() { queuedEventsSurviveTearDown(); });
	}

	void AsyncDispatcherUnitTest::newDispatcherIsInactive()
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
	void AsyncDispatcherUnitTest::setUpAfterTearDownReactivates()
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

	void AsyncDispatcherUnitTest::repeatedSetUpIsIdempotent()
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

	void AsyncDispatcherUnitTest::repeatedTearDownIsIdempotent()
	{
		cge::event::EventChannelRegistry registry;
		TestableAsyncDispatcher dispatcher("double-teardown", &registry);

		dispatcher.setUp();
		dispatcher.tearDown();
		dispatcher.tearDown();

		ASSERT_FALSE(dispatcher.active());
	}

	void AsyncDispatcherUnitTest::pushWhileInactiveIsRefused()
	{
		cge::event::EventChannelRegistry registry;
		TestableAsyncDispatcher dispatcher("inactive", &registry);
		const cge::event::EventChannel<int> &channel = registry.getChannel<int>("inactive-ch");

		ASSERT_FALSE(dispatcher.onPushEvent(channel, makeEvent(1)));
		ASSERT_FALSE(dispatcher.onPushCommand(channel, makeEvent(1)));
	}

	void AsyncDispatcherUnitTest::pushWhileInactiveQueuesNothing()
	{
		cge::event::EventChannelRegistry registry;
		TestableAsyncDispatcher dispatcher("inactive-queue", &registry);
		const cge::event::EventChannel<int> &channel = registry.getChannel<int>("inactive-queue-ch");

		dispatcher.onPushEvent(channel, makeEvent(1));
		dispatcher.onPushCommand(channel, makeEvent(1));

		ASSERT_EQUAL(dispatcher.queuedEvents(), static_cast<size_t>(0));
		ASSERT_EQUAL(dispatcher.queuedCommands(), static_cast<size_t>(0));
	}

	void AsyncDispatcherUnitTest::eventPushLandsInEventQueue()
	{
		cge::event::EventChannelRegistry registry;
		TestableAsyncDispatcher dispatcher("event-push", &registry);
		const cge::event::EventChannel<int> &channel = registry.getChannel<int>("event-push-ch");
		dispatcher.setUp();

		ASSERT_TRUE(dispatcher.onPushEvent(channel, makeEvent(1)));
		ASSERT_EQUAL(dispatcher.queuedEvents(), static_cast<size_t>(1));
	}

	void AsyncDispatcherUnitTest::commandPushLandsInCommandQueue()
	{
		cge::event::EventChannelRegistry registry;
		TestableAsyncDispatcher dispatcher("command-push", &registry);
		const cge::event::EventChannel<int> &channel = registry.getChannel<int>("command-push-ch");
		dispatcher.setUp();

		ASSERT_TRUE(dispatcher.onPushCommand(channel, makeEvent(1)));
		ASSERT_EQUAL(dispatcher.queuedCommands(), static_cast<size_t>(1));
	}

	void AsyncDispatcherUnitTest::eventPushLeavesCommandQueueEmpty()
	{
		cge::event::EventChannelRegistry registry;
		TestableAsyncDispatcher dispatcher("event-only", &registry);
		const cge::event::EventChannel<int> &channel = registry.getChannel<int>("event-only-ch");
		dispatcher.setUp();

		dispatcher.onPushEvent(channel, makeEvent(1));

		ASSERT_EQUAL(dispatcher.queuedCommands(), static_cast<size_t>(0));
	}

	void AsyncDispatcherUnitTest::commandPushLeavesEventQueueEmpty()
	{
		cge::event::EventChannelRegistry registry;
		TestableAsyncDispatcher dispatcher("command-only", &registry);
		const cge::event::EventChannel<int> &channel = registry.getChannel<int>("command-only-ch");
		dispatcher.setUp();

		dispatcher.onPushCommand(channel, makeEvent(1));

		ASSERT_EQUAL(dispatcher.queuedEvents(), static_cast<size_t>(0));
	}

	void AsyncDispatcherUnitTest::dispatchOnEmptyQueuesIsSafe()
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
	void AsyncDispatcherUnitTest::dispatchEventsDrainsWithNoListeners()
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
	void AsyncDispatcherUnitTest::queuedEventsSurviveTearDown()
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
