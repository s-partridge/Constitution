#include "listenerRegistrationTest.h"

#include <partest/assert.h>

#include "broadcaster.h"

namespace cge::test
{
	ListenerRegistrationTest::ListenerRegistrationTest(const DispatcherFlavor &flavor)
		: EventTestBase("ListenerRegistrationTest", "Listener registration, handlers, and unregister.", flavor)
	{
		partest::TestFlags flags = partest::TEST_FLAGS_INHERIT;

		addTest("RegistrationLifecycle", flags, [this]() { registrationLifecycle(); });
		addTest("Unregister", flags, [this]() { unregister(); });
		addTest("BatchedRequests", flags, [this]() { batchedRequests(); });
		addTest("OneHandlerPerChannel", flags, [this]() { oneHandlerPerChannel(); });
		addTest("Handlers", flags, [this]() { handlers(); });
	}

	// One listener walked through its whole registration life on a single
	// dispatcher; the cases run in order and share that accumulated state.
	void ListenerRegistrationTest::registrationLifecycle()
	{
		EventHarness harness(flavor(), "reg-life-dispatcher");
		const cge::event::EventChannel<int> &channel = harness.registry.getChannel<int>("reg-life");
		CountingListener listener(&harness.dispatcher());
		cge::event::BroadcasterBase broadcaster(&harness.dispatcher());
		auto handler = [&listener](const int &v) { listener.onInt(v); };

		subtest("DeliversAfterCommandDrain", [&]() {
			listener.requestRegister(channel, handler);
			harness.dispatcher().dispatchCommands();
			broadcaster.broadcast(channel, 1);
			harness.dispatcher().dispatchEvents();

			ASSERT_EQUAL(listener.received.size(), static_cast<size_t>(1));
			ASSERT_EQUAL(listener.received[0], 1);
		});

		// Whatever the second request returns, it must not double the delivery.
		// The return value itself is asserted in the listener unit tests.
		subtest("RequestAgainAfterSuccess", [&]() {
			listener.requestRegister(channel, handler);
			harness.dispatcher().dispatchCommands();

			broadcaster.broadcast(channel, 2);
			harness.dispatcher().dispatchEvents();

			ASSERT_EQUAL(listener.received.size(), static_cast<size_t>(2));
			ASSERT_EQUAL(listener.received[1], 2);
		});

		subtest("UnregisterStopsDelivery", [&]() {
			listener.requestUnregister(channel);
			harness.dispatcher().dispatchCommands();

			broadcaster.broadcast(channel, 3);
			harness.dispatcher().dispatchEvents();

			ASSERT_EQUAL(listener.received.size(), static_cast<size_t>(2));
		});
	}

	// Shared dispatcher; each case brings its own channels and listeners.
	void ListenerRegistrationTest::unregister()
	{
		EventHarness harness(flavor(), "unreg-dispatcher");
		cge::event::BroadcasterBase broadcaster(&harness.dispatcher());

		// Cycle order: commands drain before events, so an unregistration
		// requested after a broadcast still wins.
		subtest("BeatsQueuedEvents", [&]() {
			const cge::event::EventChannel<int> &channel = harness.registry.getChannel<int>("unreg-beats");
			CountingListener listener(&harness.dispatcher());

			listener.requestRegister(channel, [&listener](const int &v) { listener.onInt(v); });
			harness.dispatcher().dispatchCommands();

			broadcaster.broadcast(channel, 1);
			listener.requestUnregister(channel);

			harness.dispatcher().dispatchCommands();
			harness.dispatcher().dispatchEvents();

			ASSERT_EQUAL(listener.received.size(), static_cast<size_t>(0));
		});

		// Swap-and-pop removal must not disturb the remaining registrations.
		// Delivery order among listeners is contract-free, so none is asserted.
		subtest("OneOfSeveralListeners", [&]() {
			const cge::event::EventChannel<int> &channel = harness.registry.getChannel<int>("multi-unreg");
			CountingListener a(&harness.dispatcher());
			CountingListener b(&harness.dispatcher());
			CountingListener c(&harness.dispatcher());

			a.requestRegister(channel, [&a](const int &v) { a.onInt(v); });
			b.requestRegister(channel, [&b](const int &v) { b.onInt(v); });
			c.requestRegister(channel, [&c](const int &v) { c.onInt(v); });
			harness.dispatcher().dispatchCommands();

			b.requestUnregister(channel);
			harness.dispatcher().dispatchCommands();

			broadcaster.broadcast(channel, 7);
			harness.dispatcher().dispatchEvents();

			ASSERT_EQUAL(a.received.size(), static_cast<size_t>(1));
			ASSERT_EQUAL(b.received.size(), static_cast<size_t>(0));
			ASSERT_EQUAL(c.received.size(), static_cast<size_t>(1));
		});

		subtest("OneOfTwoChannels", [&]() {
			const cge::event::EventChannel<int> &first = harness.registry.getChannel<int>("two-ch-a");
			const cge::event::EventChannel<int> &second = harness.registry.getChannel<int>("two-ch-b");
			CountingListener listener(&harness.dispatcher());

			listener.requestRegister(first, [&listener](const int &v) { listener.onInt(v); });
			listener.requestRegister(second, [&listener](const int &v) { listener.onInt(v); });
			harness.dispatcher().dispatchCommands();

			listener.requestUnregister(first);
			harness.dispatcher().dispatchCommands();

			broadcaster.broadcast(first, 1);
			broadcaster.broadcast(second, 2);
			harness.dispatcher().dispatchEvents();

			ASSERT_EQUAL(listener.received.size(), static_cast<size_t>(1));
			ASSERT_EQUAL(listener.received[0], 2);
		});

		// Never registered: unregistering is a no-op that must leave the listener
		// able to register and receive afterwards.
		subtest("NotRegistered", [&]() {
			const cge::event::EventChannel<int> &channel = harness.registry.getChannel<int>("reg-missing");
			CountingListener listener(&harness.dispatcher());

			listener.requestUnregister(channel);
			harness.dispatcher().dispatchCommands();

			listener.requestRegister(channel, [&listener](const int &v) { listener.onInt(v); });
			harness.dispatcher().dispatchCommands();

			broadcaster.broadcast(channel, 3);
			harness.dispatcher().dispatchEvents();
			ASSERT_EQUAL(listener.received.size(), static_cast<size_t>(1));
			ASSERT_EQUAL(listener.received[0], 3);
		});
	}

	// Several requests queued before any of them is applied, then drained in one
	// command pass. The rule under test is that the last request in the batch
	// decides the outcome, whatever the earlier ones asked for. Each case brings
	// its own channel and listener so none of them inherits the last one's state.
	void ListenerRegistrationTest::batchedRequests()
	{
		EventHarness harness(flavor(), "batch-dispatcher");
		cge::event::BroadcasterBase broadcaster(&harness.dispatcher());

		// The caller asked to end up registered, so it must end up registered and
		// receiving. Currently fails: the re-register is rejected as Duplicate
		// against the still-pending first request and queues nothing.
		subtest("RegisterUnregisterRegister", [&]() {
			const cge::event::EventChannel<int> &channel = harness.registry.getChannel<int>("batch-rur");
			CountingListener listener(&harness.dispatcher());
			auto handler = [&listener](const int &v) { listener.onInt(v); };

			listener.requestRegister(channel, handler);
			listener.requestUnregister(channel);
			listener.requestRegister(channel, handler);

			harness.dispatcher().dispatchCommands();
			broadcaster.broadcast(channel, 1);
			harness.dispatcher().dispatchEvents();

			ASSERT_EQUAL(listener.received.size(), static_cast<size_t>(1));
			if(listener.received.size() == 1)
				ASSERT_EQUAL(listener.received[0], 1);
		});

		subtest("RegisterThenUnregister", [&]() {
			const cge::event::EventChannel<int> &channel = harness.registry.getChannel<int>("batch-ru");
			CountingListener listener(&harness.dispatcher());

			listener.requestRegister(channel, [&listener](const int &v) { listener.onInt(v); });
			listener.requestUnregister(channel);

			harness.dispatcher().dispatchCommands();
			broadcaster.broadcast(channel, 1);
			harness.dispatcher().dispatchEvents();

			ASSERT_EQUAL(listener.received.size(), static_cast<size_t>(0));
		});

		// The leading unregistration is a no-op against a listener that was never
		// registered, and must not poison the request that follows it.
		subtest("UnregisterThenRegister", [&]() {
			const cge::event::EventChannel<int> &channel = harness.registry.getChannel<int>("batch-ur");
			CountingListener listener(&harness.dispatcher());

			listener.requestUnregister(channel);
			listener.requestRegister(channel, [&listener](const int &v) { listener.onInt(v); });

			harness.dispatcher().dispatchCommands();
			broadcaster.broadcast(channel, 1);
			harness.dispatcher().dispatchEvents();

			ASSERT_EQUAL(listener.received.size(), static_cast<size_t>(1));
			if(listener.received.size() == 1)
				ASSERT_EQUAL(listener.received[0], 1);
		});

		subtest("RegisterTwice", [&]() {
			const cge::event::EventChannel<int> &channel = harness.registry.getChannel<int>("batch-rr");
			CountingListener listener(&harness.dispatcher());
			auto handler = [&listener](const int &v) { listener.onInt(v); };

			listener.requestRegister(channel, handler);
			listener.requestRegister(channel, handler);

			harness.dispatcher().dispatchCommands();
			broadcaster.broadcast(channel, 1);
			harness.dispatcher().dispatchEvents();

			ASSERT_EQUAL(listener.received.size(), static_cast<size_t>(1));
		});

		subtest("UnregisterTwice", [&]() {
			const cge::event::EventChannel<int> &channel = harness.registry.getChannel<int>("batch-uu");
			CountingListener listener(&harness.dispatcher());
			auto handler = [&listener](const int &v) { listener.onInt(v); };

			listener.requestRegister(channel, handler);
			harness.dispatcher().dispatchCommands();

			listener.requestUnregister(channel);
			listener.requestUnregister(channel);
			harness.dispatcher().dispatchCommands();

			broadcaster.broadcast(channel, 1);
			harness.dispatcher().dispatchEvents();
			ASSERT_EQUAL(listener.received.size(), static_cast<size_t>(0));

			// The redundant second request must not leave the listener stuck.
			listener.requestRegister(channel, handler);
			harness.dispatcher().dispatchCommands();
			broadcaster.broadcast(channel, 2);
			harness.dispatcher().dispatchEvents();

			ASSERT_EQUAL(listener.received.size(), static_cast<size_t>(1));
			if(listener.received.size() == 1)
				ASSERT_EQUAL(listener.received[0], 2);
		});
	}

	// A listener holds one handler per channel. The second request is refused as
	// a duplicate rather than adding a handler or replacing the first, so the
	// handler that was registered first is the one that stays live.
	void ListenerRegistrationTest::oneHandlerPerChannel()
	{
		EventHarness harness(flavor(), "one-handler-dispatcher");
		const cge::event::EventChannel<int> &channel = harness.registry.getChannel<int>("one-handler");
		cge::event::ListenerBase listener(&harness.dispatcher());
		cge::event::BroadcasterBase broadcaster(&harness.dispatcher());
		int firstCalls = 0;
		int secondCalls = 0;

		listener.requestRegister(channel, [&firstCalls](const int &) { ++firstCalls; });
		harness.dispatcher().dispatchCommands();

		listener.requestRegister(channel, [&secondCalls](const int &) { ++secondCalls; });
		harness.dispatcher().dispatchCommands();

		broadcaster.broadcast(channel, 1);
		harness.dispatcher().dispatchEvents();

		ASSERT_EQUAL(firstCalls, 1);
		ASSERT_EQUAL(secondCalls, 0);
	}

	// Shared dispatcher; the callback form and the listener count are what vary.
	void ListenerRegistrationTest::handlers()
	{
		EventHarness harness(flavor(), "handlers-dispatcher");
		cge::event::BroadcasterBase broadcaster(&harness.dispatcher());

		subtest("MultipleListeners", [&]() {
			const cge::event::EventChannel<int> &channel = harness.registry.getChannel<int>("reg-multi");
			CountingListener a(&harness.dispatcher());
			CountingListener b(&harness.dispatcher());

			a.requestRegister(channel, [&a](const int &v) { a.onInt(v); });
			b.requestRegister(channel, [&b](const int &v) { b.onInt(v); });
			harness.dispatcher().dispatchCommands();

			broadcaster.broadcast(channel, 9);
			harness.dispatcher().dispatchEvents();

			ASSERT_EQUAL(a.received.size(), static_cast<size_t>(1));
			ASSERT_EQUAL(b.received.size(), static_cast<size_t>(1));
			ASSERT_EQUAL(a.received[0], 9);
			ASSERT_EQUAL(b.received[0], 9);
		});
	}
}
