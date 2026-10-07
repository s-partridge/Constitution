#include "asyncDispatcherTest.h"

#include <memory>
#include <string>

#include <partest/assert.h>

#include "asyncDispatcher.h"
#include "event.h"
#include "listener.h"

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

		addTest("StartsInactive", flags, PARTEST_CTX(this) { startsInactive(ctx); });
		addTest("SetUpActivates", flags, PARTEST_CTX(this) { setUpActivates(ctx); });
		addTest("TearDownDeactivates", flags, PARTEST_CTX(this) { tearDownDeactivates(ctx); });
		addTest("Reactivates", flags, PARTEST_CTX(this) { reactivates(ctx); });
		addTest("RepeatedSetUp", flags, PARTEST_CTX(this) { repeatedSetUp(ctx); });
		addTest("RepeatedTearDown", flags, PARTEST_CTX(this) { repeatedTearDown(ctx); });

		addTest("InactiveRefused", flags, PARTEST_CTX(this) { inactiveRefused(ctx); });
		addTest("InactiveQueuesNothing", flags, PARTEST_CTX(this) { inactiveQueuesNothing(ctx); });
		addTest("InactiveUnregisterQueued", flags, PARTEST_CTX(this) { inactiveUnregisterQueued(ctx); });
		addTest("EventQueued", flags, PARTEST_CTX(this) { eventQueued(ctx); });
		addTest("CommandQueued", flags, PARTEST_CTX(this) { commandQueued(ctx); });
		addTest("EventNotInCommands", flags, PARTEST_CTX(this) { eventNotInCommands(ctx); });
		addTest("CommandNotInEvents", flags, PARTEST_CTX(this) { commandNotInEvents(ctx); });

		addTest("DrainEmpty", flags, PARTEST_CTX(this) { drainEmpty(ctx); });
		addTest("DrainNoListeners", flags, PARTEST_CTX(this) { drainNoListeners(ctx); });
		addTest("QueueSurvivesTearDown", flags, PARTEST_CTX(this) { queueSurvivesTearDown(ctx); });
	}

	void AsyncDispatcherUnitTest::startsInactive(partest::TestContext &ctx)
	{
		cge::event::EventChannelRegistry registry;
		TestableAsyncDispatcher dispatcher("new", &registry);

		ASSERT_FALSE(dispatcher.active());
	}

	void AsyncDispatcherUnitTest::setUpActivates(partest::TestContext &ctx)
	{
		cge::event::EventChannelRegistry registry;
		TestableAsyncDispatcher dispatcher("setup", &registry);

		dispatcher.setUp();

		ASSERT_TRUE(dispatcher.active());
	}

	void AsyncDispatcherUnitTest::tearDownDeactivates(partest::TestContext &ctx)
	{
		cge::event::EventChannelRegistry registry;
		TestableAsyncDispatcher dispatcher("teardown", &registry);

		dispatcher.setUp();
		dispatcher.tearDown();

		ASSERT_FALSE(dispatcher.active());
	}

	// The level transition path: a dispatcher torn down between levels has to
	// come back when the next one starts.
	void AsyncDispatcherUnitTest::reactivates(partest::TestContext &ctx)
	{
		cge::event::EventChannelRegistry registry;
		TestableAsyncDispatcher dispatcher("recycle", &registry);
		const cge::event::EventChannel<int> &channel = registry.getChannel<int>("recycle-ch");

		dispatcher.setUp();
		dispatcher.tearDown();
		dispatcher.setUp();

		ASSERT_TRUE(dispatcher.active());
		ASSERT_EQUAL(dispatcher.onPushEvent(channel, makeEvent(1)), cge::event::DispatchStatus::Pending);
	}

	void AsyncDispatcherUnitTest::repeatedSetUp(partest::TestContext &ctx)
	{
		cge::event::EventChannelRegistry registry;
		TestableAsyncDispatcher dispatcher("double-setup", &registry);
		const cge::event::EventChannel<int> &channel = registry.getChannel<int>("double-setup-ch");

		dispatcher.setUp();
		dispatcher.setUp();

		ASSERT_TRUE(dispatcher.active());
		ASSERT_EQUAL(dispatcher.onPushEvent(channel, makeEvent(1)), cge::event::DispatchStatus::Pending);
		ASSERT_EQUAL(dispatcher.queuedEvents(), 1u);
	}

	void AsyncDispatcherUnitTest::repeatedTearDown(partest::TestContext &ctx)
	{
		cge::event::EventChannelRegistry registry;
		TestableAsyncDispatcher dispatcher("double-teardown", &registry);

		dispatcher.setUp();
		dispatcher.tearDown();
		dispatcher.tearDown();

		ASSERT_FALSE(dispatcher.active());
	}

	void AsyncDispatcherUnitTest::inactiveRefused(partest::TestContext &ctx)
	{
		cge::event::EventChannelRegistry registry;
		TestableAsyncDispatcher dispatcher("inactive", &registry);
		const cge::event::EventChannel<int> &channel = registry.getChannel<int>("inactive-ch");

		ASSERT_EQUAL(dispatcher.onPushEvent(channel, makeEvent(1)), cge::event::DispatchStatus::NotReady);
		ASSERT_EQUAL(dispatcher.onPushCommand(channel, makeEvent(1)), cge::event::DispatchStatus::NotReady);
	}

	void AsyncDispatcherUnitTest::inactiveQueuesNothing(partest::TestContext &ctx)
	{
		cge::event::EventChannelRegistry registry;
		TestableAsyncDispatcher dispatcher("inactive-queue", &registry);
		const cge::event::EventChannel<int> &channel = registry.getChannel<int>("inactive-queue-ch");

		dispatcher.onPushEvent(channel, makeEvent(1));
		dispatcher.onPushCommand(channel, makeEvent(1));

		ASSERT_EQUAL(dispatcher.queuedEvents(), 0u);
		ASSERT_EQUAL(dispatcher.queuedCommands(), 0u);
	}

	// Inactivity rejects new registrations but never traps a component in the
	// dispatcher. A queued unregistration remains valid and can be drained
	// before the next setup.
	void AsyncDispatcherUnitTest::inactiveUnregisterQueued(partest::TestContext &ctx)
	{
		cge::event::EventChannelRegistry registry;
		TestableAsyncDispatcher dispatcher("inactive-unregister", &registry);
		const cge::event::EventChannel<int> &channel =
			registry.getChannel<int>("inactive-unregister-ch");
		cge::event::ListenerBase listener(&dispatcher);

		dispatcher.setUp();
		listener.requestRegister(channel, [](const int &) {});
		dispatcher.dispatchCommands();
		dispatcher.tearDown();

		ASSERT_EQUAL(listener.requestUnregister(channel),
			cge::event::DispatchStatus::Pending);
		ASSERT_EQUAL(dispatcher.queuedCommands(), 1u);

		dispatcher.dispatchCommands();
		ASSERT_EQUAL(dispatcher.queuedCommands(), 0u);
	}

	void AsyncDispatcherUnitTest::eventQueued(partest::TestContext &ctx)
	{
		cge::event::EventChannelRegistry registry;
		TestableAsyncDispatcher dispatcher("event-push", &registry);
		const cge::event::EventChannel<int> &channel = registry.getChannel<int>("event-push-ch");
		dispatcher.setUp();

		ASSERT_EQUAL(dispatcher.onPushEvent(channel, makeEvent(1)), cge::event::DispatchStatus::Pending);
		ASSERT_EQUAL(dispatcher.queuedEvents(), 1u);
	}

	void AsyncDispatcherUnitTest::commandQueued(partest::TestContext &ctx)
	{
		cge::event::EventChannelRegistry registry;
		TestableAsyncDispatcher dispatcher("command-push", &registry);
		const cge::event::EventChannel<int> &channel = registry.getChannel<int>("command-push-ch");
		dispatcher.setUp();

		ASSERT_EQUAL(dispatcher.onPushCommand(channel, makeEvent(1)), cge::event::DispatchStatus::Pending);
		ASSERT_EQUAL(dispatcher.queuedCommands(), 1u);
	}

	void AsyncDispatcherUnitTest::eventNotInCommands(partest::TestContext &ctx)
	{
		cge::event::EventChannelRegistry registry;
		TestableAsyncDispatcher dispatcher("event-only", &registry);
		const cge::event::EventChannel<int> &channel = registry.getChannel<int>("event-only-ch");
		dispatcher.setUp();

		dispatcher.onPushEvent(channel, makeEvent(1));

		ASSERT_EQUAL(dispatcher.queuedCommands(), 0u);
	}

	void AsyncDispatcherUnitTest::commandNotInEvents(partest::TestContext &ctx)
	{
		cge::event::EventChannelRegistry registry;
		TestableAsyncDispatcher dispatcher("command-only", &registry);
		const cge::event::EventChannel<int> &channel = registry.getChannel<int>("command-only-ch");
		dispatcher.setUp();

		dispatcher.onPushCommand(channel, makeEvent(1));

		ASSERT_EQUAL(dispatcher.queuedEvents(), 0u);
	}

	void AsyncDispatcherUnitTest::drainEmpty(partest::TestContext &ctx)
	{
		cge::event::EventChannelRegistry registry;
		TestableAsyncDispatcher dispatcher("empty-drain", &registry);
		dispatcher.setUp();

		dispatcher.dispatchCommands();
		dispatcher.dispatchEvents();

		ASSERT_EQUAL(dispatcher.queuedEvents(), 0u);
		ASSERT_EQUAL(dispatcher.queuedCommands(), 0u);
	}

	// Nobody is registered, so there is nowhere for the event to go. It still
	// has to leave the queue rather than accumulating there.
	void AsyncDispatcherUnitTest::drainNoListeners(partest::TestContext &ctx)
	{
		cge::event::EventChannelRegistry registry;
		TestableAsyncDispatcher dispatcher("no-listeners", &registry);
		const cge::event::EventChannel<int> &channel = registry.getChannel<int>("no-listeners-ch");
		dispatcher.setUp();

		dispatcher.onPushEvent(channel, makeEvent(1));
		ASSERT_EQUAL(dispatcher.queuedEvents(), 1u);

		dispatcher.dispatchEvents();

		ASSERT_EQUAL(dispatcher.queuedEvents(), 0u);
	}

	// Once accepted, always delivered, at its smallest testable size: tearDown
	// stops intake but must not discard what is already queued.
	void AsyncDispatcherUnitTest::queueSurvivesTearDown(partest::TestContext &ctx)
	{
		cge::event::EventChannelRegistry registry;
		TestableAsyncDispatcher dispatcher("survive", &registry);
		const cge::event::EventChannel<int> &channel = registry.getChannel<int>("survive-ch");
		dispatcher.setUp();

		dispatcher.onPushEvent(channel, makeEvent(1));
		dispatcher.tearDown();

		ASSERT_EQUAL(dispatcher.queuedEvents(), 1u);

		dispatcher.dispatchEvents();

		ASSERT_EQUAL(dispatcher.queuedEvents(), 0u);
	}
}
