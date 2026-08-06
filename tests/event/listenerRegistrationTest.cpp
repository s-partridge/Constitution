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

		subtest("RequestReturnsPending", [&]() {
			ASSERT_TRUE(listener.requestRegister(channel, handler)
				== cge::event::RegistrationResult::Pending);
		});

		subtest("DuplicateWhilePending", [&]() {
			ASSERT_TRUE(listener.requestRegister(channel, handler)
				== cge::event::RegistrationResult::Duplicate);
		});

		subtest("DeliversAfterCommandDrain", [&]() {
			harness.dispatcher().dispatchCommands();
			broadcaster.broadcast(channel, 1);
			harness.dispatcher().dispatchEvents();

			ASSERT_EQUAL(listener.received.size(), static_cast<size_t>(1));
			ASSERT_EQUAL(listener.received[0], 1);
		});

		// The pending list only guards duplicates, so a request after finalize is
		// accepted; the dispatcher then rejects it, leaving delivery single.
		subtest("RequestAgainAfterSuccess", [&]() {
			ASSERT_TRUE(listener.requestRegister(channel, handler)
				== cge::event::RegistrationResult::Pending);
			harness.dispatcher().dispatchCommands();

			broadcaster.broadcast(channel, 2);
			harness.dispatcher().dispatchEvents();

			ASSERT_EQUAL(listener.received.size(), static_cast<size_t>(2));
			ASSERT_EQUAL(listener.received[1], 2);
		});

		subtest("UnregisterStopsDelivery", [&]() {
			ASSERT_TRUE(listener.requestUnregister(channel) == cge::event::RegistrationResult::Pending);
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

		// Never registered: the request still queues, and the NotFound outcome
		// must leave the listener fully usable afterwards.
		subtest("NotRegistered", [&]() {
			const cge::event::EventChannel<int> &channel = harness.registry.getChannel<int>("reg-missing");
			CountingListener listener(&harness.dispatcher());

			ASSERT_TRUE(listener.requestUnregister(channel) == cge::event::RegistrationResult::Pending);
			ASSERT_NOTHROW(harness.dispatcher().dispatchCommands());

			listener.requestRegister(channel, [&listener](const int &v) { listener.onInt(v); });
			harness.dispatcher().dispatchCommands();

			broadcaster.broadcast(channel, 3);
			harness.dispatcher().dispatchEvents();
			ASSERT_EQUAL(listener.received.size(), static_cast<size_t>(1));
			ASSERT_EQUAL(listener.received[0], 3);
		});
	}

	// One command batch, drained FIFO, then the recovery afterwards.
	void ListenerRegistrationTest::batchedRequests()
	{
		EventHarness harness(flavor(), "batch-dispatcher");
		const cge::event::EventChannel<int> &channel = harness.registry.getChannel<int>("batch");
		CountingListener listener(&harness.dispatcher());
		cge::event::BroadcasterBase broadcaster(&harness.dispatcher());
		auto handler = [&listener](const int &v) { listener.onInt(v); };

		// The caller asked to end up registered, so it must end up registered and
		// receiving. Currently fails: the re-register is rejected as Duplicate
		// against the still-pending first request and queues nothing.
		subtest("EndsUpRegistered", [&]() {
			ASSERT_TRUE(listener.requestRegister(channel, handler)
				== cge::event::RegistrationResult::Pending);
			ASSERT_TRUE(listener.requestUnregister(channel)
				== cge::event::RegistrationResult::Pending);
			ASSERT_TRUE(listener.requestRegister(channel, handler)
				== cge::event::RegistrationResult::Pending);

			harness.dispatcher().dispatchCommands();
			broadcaster.broadcast(channel, 1);
			harness.dispatcher().dispatchEvents();

			ASSERT_EQUAL(listener.received.size(), static_cast<size_t>(1));
			if(listener.received.size() == 1)
				ASSERT_EQUAL(listener.received[0], 1);
		});
	}

	// Shared dispatcher; the callback form and the listener count are what vary.
	void ListenerRegistrationTest::handlers()
	{
		EventHarness harness(flavor(), "handlers-dispatcher");
		cge::event::BroadcasterBase broadcaster(&harness.dispatcher());

		subtest("MemberFunctionForm", [&]() {
			const cge::event::EventChannel<int> &channel = harness.registry.getChannel<int>("reg-member");
			CountingListener listener(&harness.dispatcher());

			ASSERT_TRUE(listener.requestRegister(channel, &listener, &CountingListener::onInt)
				== cge::event::RegistrationResult::Pending);
			harness.dispatcher().dispatchCommands();

			broadcaster.broadcast(channel, 77);
			harness.dispatcher().dispatchEvents();

			ASSERT_EQUAL(listener.received.size(), static_cast<size_t>(1));
			ASSERT_EQUAL(listener.received[0], 77);
		});

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
