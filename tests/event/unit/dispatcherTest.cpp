#include "dispatcherTest.h"

#include <partest/assert.h>

#include "broadcaster.h"
#include "listener.h"
#include "mockDispatcher.h"

namespace cge::test
{
	namespace
	{
		struct Counter : public cge::event::ListenerBase
		{
			int calls;
			int last;

			explicit Counter(cge::event::DispatcherBase *dispatcher)
				: ListenerBase(dispatcher)
				, calls(0)
				, last(0)
			{
			}

			void record(const int &value)
			{
				++calls;
				last = value;
			}
		};
	}

	DispatcherUnitTest::DispatcherUnitTest()
		: TestBase("DispatcherUnitTest", "Unit tests for DispatcherBase registration and delivery.")
	{
		partest::TestFlags flags = partest::TEST_FLAGS_INHERIT;

		addTest("RegisterDeferred", flags, PARTEST_CTX(this) { registerDeferred(ctx); });
		addTest("RegisterApplies", flags, PARTEST_CTX(this) { registerApplies(ctx); });
		addTest("UnregisterApplies", flags, PARTEST_CTX(this) { unregisterApplies(ctx); });
		addTest("UnregisterOne", flags, PARTEST_CTX(this) { unregisterOne(ctx); });
		addTest("MultipleListeners", flags, PARTEST_CTX(this) { multipleListeners(ctx); });
		addTest("PerChannel", flags, PARTEST_CTX(this) { perChannel(ctx); });
		addTest("NoListeners", flags, PARTEST_CTX(this) { noListeners(ctx); });
		addTest("CarriesPayload", flags, PARTEST_CTX(this) { carriesPayload(ctx); });
		addTest("EventDrainEmpties", flags, PARTEST_CTX(this) { eventDrainEmpties(ctx); });
		addTest("CommandDrainEmpties", flags, PARTEST_CTX(this) { commandDrainEmpties(ctx); });
	}

	void DispatcherUnitTest::registerDeferred(partest::TestContext &ctx)
	{
		cge::event::EventChannelRegistry registry;
		MockDispatcher dispatcher(&registry);
		dispatcher.setUp();
		const cge::event::EventChannel<int> &channel = registry.getChannel<int>("ch");
		Counter listener(&dispatcher);
		cge::event::BroadcasterBase broadcaster(&dispatcher);

		listener.requestRegister(channel, [&listener](const int &v) { listener.record(v); });
		broadcaster.broadcast(channel, 1);
		dispatcher.dispatchEvents();

		ASSERT_EQUAL(listener.calls, 0);
	}

	void DispatcherUnitTest::registerApplies(partest::TestContext &ctx)
	{
		cge::event::EventChannelRegistry registry;
		MockDispatcher dispatcher(&registry);
		dispatcher.setUp();
		const cge::event::EventChannel<int> &channel = registry.getChannel<int>("ch");
		Counter listener(&dispatcher);
		cge::event::BroadcasterBase broadcaster(&dispatcher);

		listener.requestRegister(channel, [&listener](const int &v) { listener.record(v); });
		dispatcher.dispatchCommands();
		broadcaster.broadcast(channel, 1);
		dispatcher.dispatchEvents();

		ASSERT_EQUAL(listener.calls, 1);
	}

	void DispatcherUnitTest::unregisterApplies(partest::TestContext &ctx)
	{
		cge::event::EventChannelRegistry registry;
		MockDispatcher dispatcher(&registry);
		dispatcher.setUp();
		const cge::event::EventChannel<int> &channel = registry.getChannel<int>("ch");
		Counter listener(&dispatcher);
		cge::event::BroadcasterBase broadcaster(&dispatcher);

		listener.requestRegister(channel, [&listener](const int &v) { listener.record(v); });
		dispatcher.dispatchCommands();

		listener.requestUnregister(channel);
		dispatcher.dispatchCommands();

		broadcaster.broadcast(channel, 1);
		dispatcher.dispatchEvents();

		ASSERT_EQUAL(listener.calls, 0);
	}

	// Removal is swap-and-pop, so the listener that gets relocated must still be
	// reachable afterwards.
	void DispatcherUnitTest::unregisterOne(partest::TestContext &ctx)
	{
		cge::event::EventChannelRegistry registry;
		MockDispatcher dispatcher(&registry);
		dispatcher.setUp();
		const cge::event::EventChannel<int> &channel = registry.getChannel<int>("ch");
		Counter first(&dispatcher);
		Counter second(&dispatcher);
		Counter third(&dispatcher);
		cge::event::BroadcasterBase broadcaster(&dispatcher);

		first.requestRegister(channel, [&first](const int &v) { first.record(v); });
		second.requestRegister(channel, [&second](const int &v) { second.record(v); });
		third.requestRegister(channel, [&third](const int &v) { third.record(v); });
		dispatcher.dispatchCommands();

		second.requestUnregister(channel);
		dispatcher.dispatchCommands();

		broadcaster.broadcast(channel, 1);
		dispatcher.dispatchEvents();

		ASSERT_EQUAL(first.calls, 1);
		ASSERT_EQUAL(second.calls, 0);
		ASSERT_EQUAL(third.calls, 1);
	}

	void DispatcherUnitTest::multipleListeners(partest::TestContext &ctx)
	{
		cge::event::EventChannelRegistry registry;
		MockDispatcher dispatcher(&registry);
		dispatcher.setUp();
		const cge::event::EventChannel<int> &channel = registry.getChannel<int>("ch");
		Counter first(&dispatcher);
		Counter second(&dispatcher);
		cge::event::BroadcasterBase broadcaster(&dispatcher);

		first.requestRegister(channel, [&first](const int &v) { first.record(v); });
		second.requestRegister(channel, [&second](const int &v) { second.record(v); });
		dispatcher.dispatchCommands();

		broadcaster.broadcast(channel, 9);
		dispatcher.dispatchEvents();

		ASSERT_EQUAL(first.calls, 1);
		ASSERT_EQUAL(second.calls, 1);
		ASSERT_EQUAL(first.last, 9);
		ASSERT_EQUAL(second.last, 9);
	}

	void DispatcherUnitTest::perChannel(partest::TestContext &ctx)
	{
		cge::event::EventChannelRegistry registry;
		MockDispatcher dispatcher(&registry);
		dispatcher.setUp();
		const cge::event::EventChannel<int> &watched = registry.getChannel<int>("watched");
		const cge::event::EventChannel<int> &other = registry.getChannel<int>("other");
		Counter listener(&dispatcher);
		cge::event::BroadcasterBase broadcaster(&dispatcher);

		listener.requestRegister(watched, [&listener](const int &v) { listener.record(v); });
		dispatcher.dispatchCommands();

		broadcaster.broadcast(other, 1);
		dispatcher.dispatchEvents();

		ASSERT_EQUAL(listener.calls, 0);
	}

	void DispatcherUnitTest::noListeners(partest::TestContext &ctx)
	{
		cge::event::EventChannelRegistry registry;
		MockDispatcher dispatcher(&registry);
		dispatcher.setUp();
		const cge::event::EventChannel<int> &empty = registry.getChannel<int>("empty");
		cge::event::BroadcasterBase broadcaster(&dispatcher);

		broadcaster.broadcast(empty, 1);
		dispatcher.dispatchEvents();

		ASSERT_EQUAL(dispatcher.eventCount(), static_cast<size_t>(0));
	}

	void DispatcherUnitTest::carriesPayload(partest::TestContext &ctx)
	{
		cge::event::EventChannelRegistry registry;
		MockDispatcher dispatcher(&registry);
		dispatcher.setUp();
		const cge::event::EventChannel<int> &channel = registry.getChannel<int>("ch");
		Counter listener(&dispatcher);
		cge::event::BroadcasterBase broadcaster(&dispatcher);

		listener.requestRegister(channel, [&listener](const int &v) { listener.record(v); });
		dispatcher.dispatchCommands();

		broadcaster.broadcast(channel, 123);
		dispatcher.dispatchEvents();

		ASSERT_EQUAL(listener.last, 123);
	}

	void DispatcherUnitTest::eventDrainEmpties(partest::TestContext &ctx)
	{
		cge::event::EventChannelRegistry registry;
		MockDispatcher dispatcher(&registry);
		dispatcher.setUp();
		const cge::event::EventChannel<int> &channel = registry.getChannel<int>("ch");
		cge::event::BroadcasterBase broadcaster(&dispatcher);

		broadcaster.broadcast(channel, 1);
		broadcaster.broadcast(channel, 2);
		ASSERT_EQUAL(dispatcher.eventCount(), static_cast<size_t>(2));

		dispatcher.dispatchEvents();

		ASSERT_EQUAL(dispatcher.eventCount(), static_cast<size_t>(0));
	}

	void DispatcherUnitTest::commandDrainEmpties(partest::TestContext &ctx)
	{
		cge::event::EventChannelRegistry registry;
		MockDispatcher dispatcher(&registry);
		dispatcher.setUp();
		const cge::event::EventChannel<int> &channel = registry.getChannel<int>("ch");
		Counter listener(&dispatcher);

		listener.requestRegister(channel, [&listener](const int &v) { listener.record(v); });
		ASSERT_EQUAL(dispatcher.commandCount(), static_cast<size_t>(1));

		dispatcher.dispatchCommands();

		ASSERT_EQUAL(dispatcher.commandCount(), static_cast<size_t>(0));
	}
}
