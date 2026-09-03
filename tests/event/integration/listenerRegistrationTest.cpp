#include "listenerRegistrationTest.h"

#include <partest/assert.h>

#include "broadcaster.h"

namespace cge::test
{
	ListenerRegistrationTest::ListenerRegistrationTest(const DispatcherFlavor &flavor)
		: DispatcherFlavorSuite("ListenerRegistrationTest", "Listener registration, handlers, and unregister.", flavor)
	{
		partest::TestFlags flags = partest::TEST_FLAGS_INHERIT;

		addTest("RegistrationLifecycle", flags, PARTEST_CTX(this) { registrationLifecycle(ctx); });
		addTest("Unregister", flags, PARTEST_CTX(this) { unregister(ctx); });
		addTest("BatchedRequests", flags, PARTEST_CTX(this) { batchedRequests(ctx); });
		addTest("OneHandlerPerChannel", flags, PARTEST_CTX(this) { oneHandlerPerChannel(ctx); });
		addTest("Handlers", flags, PARTEST_CTX(this) { handlers(ctx); });
	}

	// One listener walked through its whole registration life on a single
	// dispatcher; the cases run in order and share that accumulated state.
	void ListenerRegistrationTest::registrationLifecycle(partest::TestContext &ctx)
	{
		EventHarness harness(flavor(), "reg-life-dispatcher");
		const cge::event::EventChannel<int> &channel = harness.registry.getChannel<int>("reg-life");
		CountingListener listener(&harness.dispatcher());
		cge::event::BroadcasterBase broadcaster(&harness.dispatcher());
		auto handler = [&listener](const int &v) { listener.onInt(v); };

		ctx.subtest("Delivers", PARTEST_CTX(&) {
			listener.requestRegister(channel, handler);
			harness.dispatcher().dispatchCommands();
			broadcaster.broadcast(channel, 1);
			harness.dispatcher().dispatchEvents();

			ASSERT_EQUAL(listener.received.size(), static_cast<size_t>(1));
			ASSERT_EQUAL(listener.received[0], 1);
		});

		// Whatever the second request returns, it must not double the delivery.
		// The return value itself is asserted in the listener unit tests.
		ctx.subtest("RequestAgain", PARTEST_CTX(&) {
			listener.requestRegister(channel, handler);
			harness.dispatcher().dispatchCommands();

			broadcaster.broadcast(channel, 2);
			harness.dispatcher().dispatchEvents();

			ASSERT_EQUAL(listener.received.size(), static_cast<size_t>(2));
			ASSERT_EQUAL(listener.received[1], 2);
		});

		ctx.subtest("UnregisterStops", PARTEST_CTX(&) {
			listener.requestUnregister(channel);
			harness.dispatcher().dispatchCommands();

			broadcaster.broadcast(channel, 3);
			harness.dispatcher().dispatchEvents();

			ASSERT_EQUAL(listener.received.size(), static_cast<size_t>(2));
		});
	}

	// Shared dispatcher; each case brings its own channels and listeners.
	void ListenerRegistrationTest::unregister(partest::TestContext &ctx)
	{
		EventHarness harness(flavor(), "unreg-dispatcher");
		cge::event::BroadcasterBase broadcaster(&harness.dispatcher());

		// Cycle order: commands drain before events, so an unregistration
		// requested after a broadcast still wins.
		ctx.subtest("BeatsQueuedEvents", PARTEST_CTX(&) {
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
		ctx.subtest("OneOfSeveralListeners", PARTEST_CTX(&) {
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

		ctx.subtest("OneOfTwoChannels", PARTEST_CTX(&) {
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
		ctx.subtest("NotRegistered", PARTEST_CTX(&) {
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

		// Component teardown is separate from storage release. Once the queued
		// unregistration has drained, the listener may be destroyed while the
		// dispatcher continues to run without retaining a stale pointer.
		ctx.subtest("DestroyedAfterCommandDrain", PARTEST_CTX(&) {
			const cge::event::EventChannel<int> &channel = harness.registry.getChannel<int>("destroy-after-unregister");
			int deliveries = 0;

			{
				cge::event::ListenerBase listener(&harness.dispatcher());
				listener.requestRegister(channel, [&deliveries](const int &) {
					++deliveries;
				});
				harness.dispatcher().dispatchCommands();

				listener.requestUnregister(channel);
				harness.dispatcher().dispatchCommands();
			}

			broadcaster.broadcast(channel, 1);
			harness.dispatcher().dispatchEvents();

			ASSERT_EQUAL(deliveries, 0);
		});
	}

	// Several requests queued before any of them is applied, then drained in one
	// command pass. The rule under test is that the last request in the batch
	// decides the outcome, whatever the earlier ones asked for. Each case brings
	// its own channel and listener so none of them inherits the last one's state.
	void ListenerRegistrationTest::batchedRequests(partest::TestContext &ctx)
	{
		EventHarness harness(flavor(), "batch-dispatcher");
		cge::event::BroadcasterBase broadcaster(&harness.dispatcher());

		// The caller asked to end up registered, so it must end up registered and
		// receiving.
		//
		// Currently fails, and not for the reason it looks like: all three
		// requests are accepted and queued, because requestUnregister clears the
		// pending entry before forwarding. The defect is that the listener holds
		// one pending slot per channel while the queue holds three requests. The
		// first Reg command consumes the slot, the Unreg strips the handler back
		// out, and the second Reg finds no pending entry and installs nothing -
		// leaving the listener in the dispatcher's map with no handler for the
		// channel. See docs/expected-failures.md.
		ctx.subtest("RegisterUnregisterRegister",
			partest::TEST_FLAGS_INHERIT.withExpectFailure(), PARTEST_CTX(&) {
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

		ctx.subtest("RegisterThenUnregister", PARTEST_CTX(&) {
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
		ctx.subtest("UnregisterThenRegister", PARTEST_CTX(&) {
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

		ctx.subtest("RegisterTwice", PARTEST_CTX(&) {
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

		ctx.subtest("UnregisterTwice", PARTEST_CTX(&) {
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
	void ListenerRegistrationTest::oneHandlerPerChannel(partest::TestContext &ctx)
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
	void ListenerRegistrationTest::handlers(partest::TestContext &ctx)
	{
		EventHarness harness(flavor(), "handlers-dispatcher");
		cge::event::BroadcasterBase broadcaster(&harness.dispatcher());

		ctx.subtest("MultipleListeners", PARTEST_CTX(&) {
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
