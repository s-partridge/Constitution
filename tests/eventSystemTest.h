#ifndef EVENT_SYSTEM_TEST_H
#define EVENT_SYSTEM_TEST_H

#include <atomic>
#include <string>
#include <thread>
#include <type_traits>
#include <vector>

#include <partest/testbase.h>

#include "asyncDispatcher.h"
#include "broadcaster.h"
#include "event.h"
#include "listener.h"

namespace
{
	// Registry first so it outlives the dispatcher (reverse destruction order).
	struct AsyncEventHarness
	{
		cge::event::EventChannelRegistry registry;
		cge::event::AsyncDispatcher dispatcher;

		AsyncEventHarness()
			: registry()
			, dispatcher("AsyncEventTestDispatcher", &registry)
		{
			dispatcher.setUp();
		}

		~AsyncEventHarness()
		{
			dispatcher.tearDown();
		}
	};

	struct CountingListener : public cge::event::ListenerBase
	{
		std::vector<int> received;

		explicit CountingListener(cge::event::DispatcherBase *dispatcher)
			: ListenerBase(dispatcher)
		{
		}

		void onInt(const int &value)
		{
			received.push_back(value);
		}
	};
}

// ---------------------------------------------------------------------------
// EventChannelRegistry + channel construction policy
// ---------------------------------------------------------------------------
class EventChannelRegistryTest : public partest::TestBase
{
public:
	EventChannelRegistryTest()
		: TestBase("EventChannelRegistryTest", "Channel registry creation and construction policy.")
	{
		partest::TestFlags flags = partest::TEST_FLAGS_INHERIT;

		addTest("GetChannelCreatesAndReusesSameInstance", flags, [this]() { getChannelCreatesAndReusesSameInstance(); });
		addTest("GetChannelTypeMismatchThrows", flags, [this]() { getChannelTypeMismatchThrows(); });
		addTest("ChannelNotDefaultConstructibleOutsideRegistry", flags, [this]() { channelNotDefaultConstructibleOutsideRegistry(); });
		addTest("ChannelIsCopyConstructible", flags, [this]() { channelIsCopyConstructible(); });
		addTest("ChannelIsNotMoveConstructible", flags, [this]() { channelIsNotMoveConstructible(); });
		addTest("DistinctNamesGetDistinctIds", flags, [this]() { distinctNamesGetDistinctIds(); });
	}

	void getChannelCreatesAndReusesSameInstance()
	{
		cge::event::EventChannelRegistry registry;
		const cge::event::EventChannel<int> &first = registry.getChannel<int>("alpha");
		const cge::event::EventChannel<int> &second = registry.getChannel<int>("alpha");

		ASSERT_EQUAL(first.id(), second.id());
		ASSERT_TRUE(&first == &second);
	}

	void getChannelTypeMismatchThrows()
	{
		cge::event::EventChannelRegistry registry;
		registry.getChannel<int>("typed");

		ASSERT_THROWS(std::runtime_error, registry.getChannel<std::string>("typed"));
	}

	void channelNotDefaultConstructibleOutsideRegistry()
	{
		// Protected default ctor: new channels must come from the registry.
		ASSERT_FALSE(std::is_default_constructible<cge::event::EventChannel<int>>::value);
		ASSERT_FALSE(std::is_default_constructible<cge::event::EventChannelBase>::value);
	}

	void channelIsCopyConstructible()
	{
		ASSERT_TRUE(std::is_copy_constructible<cge::event::EventChannel<int>>::value);

		cge::event::EventChannelRegistry registry;
		const cge::event::EventChannel<int> &original = registry.getChannel<int>("copyable");
		cge::event::EventChannel<int> copy(original);

		ASSERT_EQUAL(copy.id(), original.id());
	}

	void channelIsNotMoveConstructible()
	{
		ASSERT_FALSE(std::is_move_constructible<cge::event::EventChannel<int>>::value);
		ASSERT_FALSE(std::is_move_constructible<cge::event::EventChannelBase>::value);
	}

	void distinctNamesGetDistinctIds()
	{
		cge::event::EventChannelRegistry registry;
		const cge::event::EventChannel<int> &a = registry.getChannel<int>("a");
		const cge::event::EventChannel<int> &b = registry.getChannel<int>("b");

		ASSERT_NOT_EQUAL(a.id(), b.id());
	}
};

// ---------------------------------------------------------------------------
// AsyncDispatcher
// ---------------------------------------------------------------------------
class AsyncDispatcherTest : public partest::TestBase
{
public:
	AsyncDispatcherTest()
		: TestBase("AsyncDispatcherTest", "Async dispatcher push, drain, and lifecycle.")
	{
		partest::TestFlags flags = partest::TEST_FLAGS_INHERIT;

		addTest("DispatchOnEmptyQueuesIsNoOp", flags, [this]() { dispatchOnEmptyQueuesIsNoOp(); });
		addTest("RejectsPushBeforeSetUp", flags, [this]() { rejectsPushBeforeSetUp(); });
		addTest("RejectsPushAfterTearDown", flags, [this]() { rejectsPushAfterTearDown(); });
		addTest("EventsDoNotRunUntilDispatchEvents", flags, [this]() { eventsDoNotRunUntilDispatchEvents(); });
		addTest("RegistrationRequiresDispatchCommands", flags, [this]() { registrationRequiresDispatchCommands(); });
		addTest("DrainUntilEmptyHandlesReentryBroadcasts", flags, [this]() { drainUntilEmptyHandlesReentryBroadcasts(); });
		addTest("ConcurrentProducersAreDeliveredOnDispatch", flags, [this]() { concurrentProducersAreDeliveredOnDispatch(); });
		addTest("UnknownCommandsAreConsumedWithoutListenerDelivery", flags, [this]() { unknownCommandsAreConsumedWithoutListenerDelivery(); });
	}

	void dispatchOnEmptyQueuesIsNoOp()
	{
		AsyncEventHarness harness;
		ASSERT_NOTHROW(harness.dispatcher.dispatchCommands());
		ASSERT_NOTHROW(harness.dispatcher.dispatchEvents());
	}

	void rejectsPushBeforeSetUp()
	{
		cge::event::EventChannelRegistry registry;
		cge::event::AsyncDispatcher dispatcher("pre-setup", &registry);
		// deliberately not setUp()

		const cge::event::EventChannel<int> &channel = registry.getChannel<int>("pre");
		cge::event::ListenerBase listener(&dispatcher);
		cge::event::RegistrationResult result = listener.requestRegister(channel, [](const int &) {});

		ASSERT_TRUE(result == cge::event::RegistrationResult::Failure);
	}

	void rejectsPushAfterTearDown()
	{
		cge::event::EventChannelRegistry registry;
		cge::event::AsyncDispatcher dispatcher("post-teardown", &registry);
		dispatcher.setUp();
		dispatcher.tearDown();

		const cge::event::EventChannel<int> &channel = registry.getChannel<int>("post");
		cge::event::ListenerBase listener(&dispatcher);
		cge::event::RegistrationResult result = listener.requestRegister(channel, [](const int &) {});

		ASSERT_TRUE(result == cge::event::RegistrationResult::Failure);

		cge::event::BroadcasterBase broadcaster(&dispatcher);
		ASSERT_NOTHROW(broadcaster.broadcast(channel, 1));
	}

	void eventsDoNotRunUntilDispatchEvents()
	{
		AsyncEventHarness harness;
		const cge::event::EventChannel<int> &channel = harness.registry.getChannel<int>("deferred");
		CountingListener listener(&harness.dispatcher);

		ASSERT_TRUE(listener.requestRegister(channel, [&listener](const int &v) { listener.onInt(v); })
			== cge::event::RegistrationResult::Pending);
		harness.dispatcher.dispatchCommands();

		cge::event::BroadcasterBase broadcaster(&harness.dispatcher);
		broadcaster.broadcast(channel, 42);

		ASSERT_EQUAL(listener.received.size(), static_cast<size_t>(0));

		harness.dispatcher.dispatchEvents();

		ASSERT_EQUAL(listener.received.size(), static_cast<size_t>(1));
		ASSERT_EQUAL(listener.received[0], 42);
	}

	void registrationRequiresDispatchCommands()
	{
		AsyncEventHarness harness;
		const cge::event::EventChannel<int> &channel = harness.registry.getChannel<int>("needs-cmd");
		CountingListener listener(&harness.dispatcher);

		listener.requestRegister(channel, [&listener](const int &v) { listener.onInt(v); });

		cge::event::BroadcasterBase broadcaster(&harness.dispatcher);
		broadcaster.broadcast(channel, 7);
		// Registration still pending; listener map not updated yet.
		harness.dispatcher.dispatchEvents();
		ASSERT_EQUAL(listener.received.size(), static_cast<size_t>(0));

		harness.dispatcher.dispatchCommands();
		broadcaster.broadcast(channel, 8);
		harness.dispatcher.dispatchEvents();

		ASSERT_EQUAL(listener.received.size(), static_cast<size_t>(1));
		ASSERT_EQUAL(listener.received[0], 8);
	}

	void drainUntilEmptyHandlesReentryBroadcasts()
	{
		AsyncEventHarness harness;
		const cge::event::EventChannel<int> &channel = harness.registry.getChannel<int>("reentry");
		CountingListener listener(&harness.dispatcher);
		cge::event::BroadcasterBase broadcaster(&harness.dispatcher);

		listener.requestRegister(channel, [&](const int &v) {
			listener.onInt(v);
			if(v == 1)
				broadcaster.broadcast(channel, 2);
		});
		harness.dispatcher.dispatchCommands();

		broadcaster.broadcast(channel, 1);
		harness.dispatcher.dispatchEvents();

		ASSERT_EQUAL(listener.received.size(), static_cast<size_t>(2));
		ASSERT_EQUAL(listener.received[0], 1);
		ASSERT_EQUAL(listener.received[1], 2);
	}

	void concurrentProducersAreDeliveredOnDispatch()
	{
		AsyncEventHarness harness;
		const cge::event::EventChannel<int> &channel = harness.registry.getChannel<int>("mt");
		std::atomic<int> total(0);

		cge::event::ListenerBase listener(&harness.dispatcher);
		listener.requestRegister(channel, [&total](const int &v) { total.fetch_add(v); });
		harness.dispatcher.dispatchCommands();

		cge::event::BroadcasterBase broadcaster(&harness.dispatcher);
		const int producers = 4;
		const int perProducer = 50;

		std::vector<std::thread> threads;
		for(int p = 0; p < producers; ++p)
		{
			threads.emplace_back([&broadcaster, &channel, perProducer]() {
				for(int i = 0; i < perProducer; ++i)
					broadcaster.broadcast(channel, 1);
			});
		}
		for(std::thread &t : threads)
			t.join();

		harness.dispatcher.dispatchEvents();

		ASSERT_EQUAL(total.load(), producers * perProducer);
	}

	// Commander/user commands are drained by dispatchCommands but are not fanned out
	// to listeners (only registration/unregistration channels are handled).
	void unknownCommandsAreConsumedWithoutListenerDelivery()
	{
		AsyncEventHarness harness;
		const cge::event::EventChannel<int> &channel = harness.registry.getChannel<int>("cmd-only");
		CountingListener listener(&harness.dispatcher);

		listener.requestRegister(channel, [&listener](const int &v) { listener.onInt(v); });
		harness.dispatcher.dispatchCommands();

		cge::event::CommanderBase commander(&harness.dispatcher);
		commander.command(channel, 99);
		harness.dispatcher.dispatchCommands();
		harness.dispatcher.dispatchEvents();

		ASSERT_EQUAL(listener.received.size(), static_cast<size_t>(0));
	}
};

// ---------------------------------------------------------------------------
// BroadcasterBase
// ---------------------------------------------------------------------------
class BroadcasterBaseTest : public partest::TestBase
{
public:
	BroadcasterBaseTest()
		: TestBase("BroadcasterBaseTest", "Public broadcaster payload delivery.")
	{
		partest::TestFlags flags = partest::TEST_FLAGS_INHERIT;

		addTest("BroadcastDeliversTypedPayload", flags, [this]() { broadcastDeliversTypedPayload(); });
		addTest("BroadcastToChannelWithNoListenersDoesNotThrow", flags, [this]() { broadcastToChannelWithNoListenersDoesNotThrow(); });
		addTest("MultipleBroadcastsPreserveOrder", flags, [this]() { multipleBroadcastsPreserveOrder(); });
		addTest("StringPayloadDoesNotSlice", flags, [this]() { stringPayloadDoesNotSlice(); });
	}

	void broadcastDeliversTypedPayload()
	{
		AsyncEventHarness harness;
		const cge::event::EventChannel<int> &channel = harness.registry.getChannel<int>("bc-int");
		CountingListener listener(&harness.dispatcher);

		listener.requestRegister(channel, [&listener](const int &v) { listener.onInt(v); });
		harness.dispatcher.dispatchCommands();

		cge::event::BroadcasterBase broadcaster(&harness.dispatcher);
		broadcaster.broadcast(channel, 123);
		harness.dispatcher.dispatchEvents();

		ASSERT_EQUAL(listener.received.size(), static_cast<size_t>(1));
		ASSERT_EQUAL(listener.received[0], 123);
	}

	void broadcastToChannelWithNoListenersDoesNotThrow()
	{
		AsyncEventHarness harness;
		const cge::event::EventChannel<int> &channel = harness.registry.getChannel<int>("bc-empty");
		cge::event::BroadcasterBase broadcaster(&harness.dispatcher);

		ASSERT_NOTHROW(broadcaster.broadcast(channel, 1));
		ASSERT_NOTHROW(harness.dispatcher.dispatchEvents());
	}

	void multipleBroadcastsPreserveOrder()
	{
		AsyncEventHarness harness;
		const cge::event::EventChannel<int> &channel = harness.registry.getChannel<int>("bc-order");
		CountingListener listener(&harness.dispatcher);

		listener.requestRegister(channel, [&listener](const int &v) { listener.onInt(v); });
		harness.dispatcher.dispatchCommands();

		cge::event::BroadcasterBase broadcaster(&harness.dispatcher);
		broadcaster.broadcast(channel, 1);
		broadcaster.broadcast(channel, 2);
		broadcaster.broadcast(channel, 3);
		harness.dispatcher.dispatchEvents();

		ASSERT_EQUAL(listener.received.size(), static_cast<size_t>(3));
		ASSERT_EQUAL(listener.received[0], 1);
		ASSERT_EQUAL(listener.received[1], 2);
		ASSERT_EQUAL(listener.received[2], 3);
	}

	void stringPayloadDoesNotSlice()
	{
		AsyncEventHarness harness;
		const cge::event::EventChannel<std::string> &channel = harness.registry.getChannel<std::string>("bc-str");
		std::string got;

		cge::event::ListenerBase listener(&harness.dispatcher);
		listener.requestRegister(channel, [&got](const std::string &s) { got = s; });
		harness.dispatcher.dispatchCommands();

		cge::event::BroadcasterBase broadcaster(&harness.dispatcher);
		broadcaster.broadcast(channel, std::string("hello-event"));
		harness.dispatcher.dispatchEvents();

		ASSERT_EQUAL(got, std::string("hello-event"));
	}
};

// ---------------------------------------------------------------------------
// CommanderBase
// ---------------------------------------------------------------------------
class CommanderBaseTest : public partest::TestBase
{
public:
	CommanderBaseTest()
		: TestBase("CommanderBaseTest", "Public commander enqueue behavior.")
	{
		partest::TestFlags flags = partest::TEST_FLAGS_INHERIT;

		addTest("CommandDoesNotDeliverToChannelListeners", flags, [this]() { commandDoesNotDeliverToChannelListeners(); });
		addTest("CommandAfterTearDownDoesNotThrow", flags, [this]() { commandAfterTearDownDoesNotThrow(); });
		addTest("CommandQueueDoesNotCrossIntoEventListeners", flags, [this]() { commandQueueDoesNotCrossIntoEventListeners(); });
	}

	// Current design: dispatchCommands only applies registration/unregistration.
	// Arbitrary CommanderBase payloads are dequeued and discarded.
	void commandDoesNotDeliverToChannelListeners()
	{
		AsyncEventHarness harness;
		const cge::event::EventChannel<int> &channel = harness.registry.getChannel<int>("cmd-int");
		CountingListener listener(&harness.dispatcher);

		listener.requestRegister(channel, [&listener](const int &v) { listener.onInt(v); });
		harness.dispatcher.dispatchCommands();

		cge::event::CommanderBase commander(&harness.dispatcher);
		commander.command(channel, 55);
		harness.dispatcher.dispatchCommands();
		harness.dispatcher.dispatchEvents();

		ASSERT_EQUAL(listener.received.size(), static_cast<size_t>(0));
	}

	void commandAfterTearDownDoesNotThrow()
	{
		cge::event::EventChannelRegistry registry;
		cge::event::AsyncDispatcher dispatcher("cmd-td", &registry);
		dispatcher.setUp();
		const cge::event::EventChannel<int> &channel = registry.getChannel<int>("cmd-td-ch");
		cge::event::CommanderBase commander(&dispatcher);
		dispatcher.tearDown();

		ASSERT_NOTHROW(commander.command(channel, 1));
	}

	void commandQueueDoesNotCrossIntoEventListeners()
	{
		AsyncEventHarness harness;
		const cge::event::EventChannel<int> &channel = harness.registry.getChannel<int>("cmd-vs-evt");
		CountingListener listener(&harness.dispatcher);

		listener.requestRegister(channel, [&listener](const int &v) { listener.onInt(v); });
		harness.dispatcher.dispatchCommands();

		cge::event::CommanderBase commander(&harness.dispatcher);
		cge::event::BroadcasterBase broadcaster(&harness.dispatcher);

		commander.command(channel, 1);
		broadcaster.broadcast(channel, 2);
		// Drain commands first (drops the command payload), then events.
		harness.dispatcher.dispatchCommands();
		harness.dispatcher.dispatchEvents();

		ASSERT_EQUAL(listener.received.size(), static_cast<size_t>(1));
		ASSERT_EQUAL(listener.received[0], 2);
	}
};

// ---------------------------------------------------------------------------
// ListenerBase
// ---------------------------------------------------------------------------
class ListenerBaseTest : public partest::TestBase
{
public:
	ListenerBaseTest()
		: TestBase("ListenerBaseTest", "Listener registration, handlers, and unregister.")
	{
		partest::TestFlags flags = partest::TEST_FLAGS_INHERIT;

		addTest("RequestRegisterReturnsPendingWhenActive", flags, [this]() { requestRegisterReturnsPendingWhenActive(); });
		addTest("DuplicatePendingRegisterReturnsDuplicate", flags, [this]() { duplicatePendingRegisterReturnsDuplicate(); });
		addTest("MemberFunctionCallbackFormWorks", flags, [this]() { memberFunctionCallbackFormWorks(); });
		addTest("MultipleListenersReceiveSameEvent", flags, [this]() { multipleListenersReceiveSameEvent(); });
		addTest("UnregisterStopsFurtherDelivery", flags, [this]() { unregisterStopsFurtherDelivery(); });
		addTest("UnregisterNotRegisteredStillPendingThenNotFound", flags, [this]() { unregisterNotRegisteredStillPendingThenNotFound(); });
		addTest("SecondRegisterAfterSuccessStillAcceptedAsPending", flags, [this]() { secondRegisterAfterSuccessStillAcceptedAsPending(); });
	}

	void requestRegisterReturnsPendingWhenActive()
	{
		AsyncEventHarness harness;
		const cge::event::EventChannel<int> &channel = harness.registry.getChannel<int>("reg-pending");
		cge::event::ListenerBase listener(&harness.dispatcher);

		cge::event::RegistrationResult result = listener.requestRegister(channel, [](const int &) {});
		ASSERT_TRUE(result == cge::event::RegistrationResult::Pending);
	}

	void duplicatePendingRegisterReturnsDuplicate()
	{
		AsyncEventHarness harness;
		const cge::event::EventChannel<int> &channel = harness.registry.getChannel<int>("reg-dup");
		cge::event::ListenerBase listener(&harness.dispatcher);

		ASSERT_TRUE(listener.requestRegister(channel, [](const int &) {})
			== cge::event::RegistrationResult::Pending);
		ASSERT_TRUE(listener.requestRegister(channel, [](const int &) {})
			== cge::event::RegistrationResult::Duplicate);
	}

	void memberFunctionCallbackFormWorks()
	{
		AsyncEventHarness harness;
		const cge::event::EventChannel<int> &channel = harness.registry.getChannel<int>("reg-member");
		CountingListener listener(&harness.dispatcher);

		ASSERT_TRUE(listener.requestRegister(channel, &listener, &CountingListener::onInt)
			== cge::event::RegistrationResult::Pending);
		harness.dispatcher.dispatchCommands();

		cge::event::BroadcasterBase broadcaster(&harness.dispatcher);
		broadcaster.broadcast(channel, 77);
		harness.dispatcher.dispatchEvents();

		ASSERT_EQUAL(listener.received.size(), static_cast<size_t>(1));
		ASSERT_EQUAL(listener.received[0], 77);
	}

	void multipleListenersReceiveSameEvent()
	{
		AsyncEventHarness harness;
		const cge::event::EventChannel<int> &channel = harness.registry.getChannel<int>("reg-multi");
		CountingListener a(&harness.dispatcher);
		CountingListener b(&harness.dispatcher);

		a.requestRegister(channel, [&a](const int &v) { a.onInt(v); });
		b.requestRegister(channel, [&b](const int &v) { b.onInt(v); });
		harness.dispatcher.dispatchCommands();

		cge::event::BroadcasterBase broadcaster(&harness.dispatcher);
		broadcaster.broadcast(channel, 9);
		harness.dispatcher.dispatchEvents();

		ASSERT_EQUAL(a.received.size(), static_cast<size_t>(1));
		ASSERT_EQUAL(b.received.size(), static_cast<size_t>(1));
		ASSERT_EQUAL(a.received[0], 9);
		ASSERT_EQUAL(b.received[0], 9);
	}

	void unregisterStopsFurtherDelivery()
	{
		AsyncEventHarness harness;
		const cge::event::EventChannel<int> &channel = harness.registry.getChannel<int>("reg-unreg");
		CountingListener listener(&harness.dispatcher);

		listener.requestRegister(channel, [&listener](const int &v) { listener.onInt(v); });
		harness.dispatcher.dispatchCommands();

		cge::event::BroadcasterBase broadcaster(&harness.dispatcher);
		broadcaster.broadcast(channel, 1);
		harness.dispatcher.dispatchEvents();
		ASSERT_EQUAL(listener.received.size(), static_cast<size_t>(1));

		ASSERT_TRUE(listener.requestUnregister(channel) == cge::event::RegistrationResult::Pending);
		harness.dispatcher.dispatchCommands();

		broadcaster.broadcast(channel, 2);
		harness.dispatcher.dispatchEvents();
		ASSERT_EQUAL(listener.received.size(), static_cast<size_t>(1));
	}

	void unregisterNotRegisteredStillPendingThenNotFound()
	{
		AsyncEventHarness harness;
		const cge::event::EventChannel<int> &channel = harness.registry.getChannel<int>("reg-missing");
		cge::event::ListenerBase listener(&harness.dispatcher);

		// No register first — unregistration is still queued as a command.
		ASSERT_TRUE(listener.requestUnregister(channel) == cge::event::RegistrationResult::Pending);
		ASSERT_NOTHROW(harness.dispatcher.dispatchCommands());
	}

	// Pending-list only guards duplicates; a second request after finalize may queue
	// again and finalize as Duplicate on the dispatcher map (handler already present).
	void secondRegisterAfterSuccessStillAcceptedAsPending()
	{
		AsyncEventHarness harness;
		const cge::event::EventChannel<int> &channel = harness.registry.getChannel<int>("reg-again");
		CountingListener listener(&harness.dispatcher);

		listener.requestRegister(channel, [&listener](const int &v) { listener.onInt(v); });
		harness.dispatcher.dispatchCommands();

		cge::event::RegistrationResult again = listener.requestRegister(channel, [&listener](const int &v) { listener.onInt(v); });
		ASSERT_TRUE(again == cge::event::RegistrationResult::Pending);
		harness.dispatcher.dispatchCommands();

		cge::event::BroadcasterBase broadcaster(&harness.dispatcher);
		broadcaster.broadcast(channel, 1);
		harness.dispatcher.dispatchEvents();

		// Dispatcher rejects duplicate listener pointer on the channel; only one delivery.
		ASSERT_EQUAL(listener.received.size(), static_cast<size_t>(1));
	}
};

#endif
