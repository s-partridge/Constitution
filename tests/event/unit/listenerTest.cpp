#include "listenerTest.h"

#include <string>

#include <partest/assert.h>

#include "listener.h"
#include "mockDispatcher.h"

namespace cge::test
{
	namespace
	{
		// Concrete listener for the member function overload.
		struct Recorder : public cge::event::ListenerBase
		{
			int last;
			int calls;

			explicit Recorder(cge::event::DispatcherBase *dispatcher)
				: ListenerBase(dispatcher)
				, last(0)
				, calls(0)
			{
			}

			void onValue(const int &value)
			{
				last = value;
				++calls;
			}
		};

		// Records every rejection the dispatcher reports back after a request was
		// accepted, so the hooks themselves can be asserted.
		struct FailureRecorder : public cge::event::ListenerBase
		{
			int registerFailures;
			int unregisterFailures;
			cge::event::DispatchStatus lastStatus;
			cge::event::ChannelId lastChannel;
			cge::event::HandlerFunction lastRejectedHandler;

			explicit FailureRecorder(cge::event::DispatcherBase *dispatcher)
				: ListenerBase(dispatcher)
				, registerFailures(0)
				, unregisterFailures(0)
				, lastStatus(cge::event::DispatchStatus::Success)
				, lastChannel(cge::event::InvalidChannelId)
			{
			}

		protected:
			void onRegisterFailed(cge::event::HandlerPair &&handlerPair, cge::event::DispatchStatus status) override
			{
				++registerFailures;
				lastStatus = status;
				lastChannel = handlerPair.first;
				lastRejectedHandler = std::move(handlerPair.second);
			}

			void onUnregisterFailed(cge::event::ChannelId channelId, cge::event::DispatchStatus status) override
			{
				++unregisterFailures;
				lastStatus = status;
				lastChannel = channelId;
			}
		};
	}

	ListenerUnitTest::ListenerUnitTest()
		: TestBase("ListenerUnitTest", "Unit tests for ListenerBase against a mock dispatcher.")
	{
		partest::TestFlags flags = partest::TEST_FLAGS_INHERIT;

		addTest("ReturnsPending", flags, PARTEST_CTX(this) { returnsPending(ctx); });
		addTest("QueuesOneCommand", flags, PARTEST_CTX(this) { queuesOneCommand(ctx); });
		addTest("DuplicatePending", flags, PARTEST_CTX(this) { duplicatePending(ctx); });
		addTest("DuplicateQueuesCommand", flags, PARTEST_CTX(this) { duplicateQueuesCommand(ctx); });
		addTest("DuplicateKeepsFirst", flags, PARTEST_CTX(this) { duplicateKeepsFirst(ctx); });
		addTest("RefusedResult", flags, PARTEST_CTX(this) { refusedResult(ctx); });
		addTest("RefusedRetry", flags, PARTEST_CTX(this) { refusedRetry(ctx); });
		addTest("RefusedUnregister", flags, PARTEST_CTX(this) { refusedUnregister(ctx); });
		addTest("RegisterAfterUnregister", flags, PARTEST_CTX(this) { registerAfterUnregister(ctx); });
		addTest("UnregisterUnknown", flags, PARTEST_CTX(this) { unregisterUnknown(ctx); });
		addTest("DuplicateReported", flags, PARTEST_CTX(this) { duplicateReported(ctx); });
		addTest("UnregisterUnknownReported", flags, PARTEST_CTX(this) { unregisterUnknownReported(ctx); });
		addTest("SuccessNotReported", flags, PARTEST_CTX(this) { successNotReported(ctx); });
		addTest("ReregisterAfterDrain", flags, PARTEST_CTX(this) { reregisterAfterDrain(ctx); });

		addTest("HandlerNotLiveYet", flags, PARTEST_CTX(this) { handlerNotLiveYet(ctx); });
		addTest("InvokesHandler", flags, PARTEST_CTX(this) { invokesHandler(ctx); });
		addTest("PassesPayload", flags, PARTEST_CTX(this) { passesPayload(ctx); });
		addTest("SelectsByChannel", flags, PARTEST_CTX(this) { selectsByChannel(ctx); });
		addTest("SelectsAcrossTypes", flags, PARTEST_CTX(this) { selectsAcrossTypes(ctx); });
		addTest("IgnoresUnknownChannel", flags, PARTEST_CTX(this) { ignoresUnknownChannel(ctx); });
		addTest("MemberFunctionForm", flags, PARTEST_CTX(this) { memberFunctionForm(ctx); });
	}

	void ListenerUnitTest::returnsPending(partest::TestContext &ctx)
	{
		cge::event::EventChannelRegistry registry;
		MockDispatcher dispatcher(&registry);
		dispatcher.setUp();
		const cge::event::EventChannel<int> &channel = registry.getChannel<int>("ch");
		cge::event::ListenerBase listener(&dispatcher);

		ASSERT_EQUAL(listener.requestRegister(channel, [](const int &) {}), cge::event::DispatchStatus::Pending);
	}

	void ListenerUnitTest::queuesOneCommand(partest::TestContext &ctx)
	{
		cge::event::EventChannelRegistry registry;
		MockDispatcher dispatcher(&registry);
		dispatcher.setUp();
		const cge::event::EventChannel<int> &channel = registry.getChannel<int>("ch");
		cge::event::ListenerBase listener(&dispatcher);

		listener.requestRegister(channel, [](const int &) {});

		ASSERT_EQUAL(dispatcher.commandCount(), 1u);
	}

	// The listener does not judge duplicates at request time. Its own state only
	// changes when the dispatcher applies a request, so anything it checked here
	// could already be stale. A second request is queued like any other.
	void ListenerUnitTest::duplicatePending(partest::TestContext &ctx)
	{
		cge::event::EventChannelRegistry registry;
		MockDispatcher dispatcher(&registry);
		dispatcher.setUp();
		const cge::event::EventChannel<int> &channel = registry.getChannel<int>("ch");
		cge::event::ListenerBase listener(&dispatcher);

		listener.requestRegister(channel, [](const int &) {});

		ASSERT_EQUAL(listener.requestRegister(channel, [](const int &) {}), cge::event::DispatchStatus::Pending);
	}

	void ListenerUnitTest::duplicateQueuesCommand(partest::TestContext &ctx)
	{
		cge::event::EventChannelRegistry registry;
		MockDispatcher dispatcher(&registry);
		dispatcher.setUp();
		const cge::event::EventChannel<int> &channel = registry.getChannel<int>("ch");
		cge::event::ListenerBase listener(&dispatcher);

		listener.requestRegister(channel, [](const int &) {});
		listener.requestRegister(channel, [](const int &) {});

		ASSERT_EQUAL(dispatcher.commandCount(), 2u);
	}

	// The duplicate is resolved when the dispatcher applies it: the first handler
	// stays live and the second is discarded.
	void ListenerUnitTest::duplicateKeepsFirst(partest::TestContext &ctx)
	{
		cge::event::EventChannelRegistry registry;
		MockDispatcher dispatcher(&registry);
		dispatcher.setUp();
		const cge::event::EventChannel<int> &channel = registry.getChannel<int>("ch");
		cge::event::ListenerBase listener(&dispatcher);
		int firstCalls = 0;
		int secondCalls = 0;

		listener.requestRegister(channel, [&firstCalls](const int &) { ++firstCalls; });
		listener.requestRegister(channel, [&secondCalls](const int &) { ++secondCalls; });
		dispatcher.dispatchCommands();

		cge::event::Event<int> event(1);
		listener.onEvent(channel.id(), event);

		ASSERT_EQUAL(firstCalls, 1);
		ASSERT_EQUAL(secondCalls, 0);
	}

	void ListenerUnitTest::refusedResult(partest::TestContext &ctx)
	{
		cge::event::EventChannelRegistry registry;
		MockDispatcher dispatcher(&registry);
		dispatcher.setUp();
		dispatcher.setAcceptPushes(false);
		const cge::event::EventChannel<int> &channel = registry.getChannel<int>("ch");
		cge::event::ListenerBase listener(&dispatcher);

		ASSERT_TRUE(listener.requestRegister(channel, [](const int &) {})
			== cge::event::DispatchStatus::Failure);
	}

	// A refused registration must leave the listener able to try again.
	void ListenerUnitTest::refusedRetry(partest::TestContext &ctx)
	{
		cge::event::EventChannelRegistry registry;
		MockDispatcher dispatcher(&registry);
		dispatcher.setUp();
		const cge::event::EventChannel<int> &channel = registry.getChannel<int>("ch");
		cge::event::ListenerBase listener(&dispatcher);

		dispatcher.setAcceptPushes(false);
		listener.requestRegister(channel, [](const int &) {});
		dispatcher.setAcceptPushes(true);

		ASSERT_EQUAL(listener.requestRegister(channel, [](const int &) {}), cge::event::DispatchStatus::Pending);
	}

	// A refused unregistration changes nothing: the handler stays live.
	void ListenerUnitTest::refusedUnregister(partest::TestContext &ctx)
	{
		cge::event::EventChannelRegistry registry;
		MockDispatcher dispatcher(&registry);
		dispatcher.setUp();
		const cge::event::EventChannel<int> &channel = registry.getChannel<int>("ch");
		cge::event::ListenerBase listener(&dispatcher);
		int calls = 0;

		listener.requestRegister(channel, [&calls](const int &) { ++calls; });
		dispatcher.dispatchCommands();

		dispatcher.setAcceptPushes(false);
		ASSERT_EQUAL(listener.requestUnregister(channel), cge::event::DispatchStatus::Failure);
		dispatcher.setAcceptPushes(true);
		dispatcher.dispatchCommands();

		cge::event::Event<int> event(1);
		listener.onEvent(channel.id(), event);

		ASSERT_EQUAL(calls, 1);
	}

	void ListenerUnitTest::registerAfterUnregister(partest::TestContext &ctx)
	{
		cge::event::EventChannelRegistry registry;
		MockDispatcher dispatcher(&registry);
		dispatcher.setUp();
		const cge::event::EventChannel<int> &channel = registry.getChannel<int>("ch");
		cge::event::ListenerBase listener(&dispatcher);

		listener.requestRegister(channel, [](const int &) {});
		listener.requestUnregister(channel);

		ASSERT_EQUAL(listener.requestRegister(channel, [](const int &) {}), cge::event::DispatchStatus::Pending);
	}

	void ListenerUnitTest::unregisterUnknown(partest::TestContext &ctx)
	{
		cge::event::EventChannelRegistry registry;
		MockDispatcher dispatcher(&registry);
		dispatcher.setUp();
		const cge::event::EventChannel<int> &channel = registry.getChannel<int>("ch");
		cge::event::ListenerBase listener(&dispatcher);

		// The listener does not judge the request; the dispatcher does, when it
		// is applied, and reports the outcome through onUnregisterFailed.
		ASSERT_EQUAL(listener.requestUnregister(channel), cge::event::DispatchStatus::Pending);
		dispatcher.dispatchCommands();

		// It must not poison the listener.
		ASSERT_EQUAL(listener.requestRegister(channel, [](const int &) {}), cge::event::DispatchStatus::Pending);
	}

	// A duplicate registration is reported through onRegisterFailed when the
	// dispatcher applies it, and the handler handed back is the rejected one,
	// not the live one.
	void ListenerUnitTest::duplicateReported(partest::TestContext &ctx)
	{
		cge::event::EventChannelRegistry registry;
		MockDispatcher dispatcher(&registry);
		dispatcher.setUp();
		const cge::event::EventChannel<int> &channel = registry.getChannel<int>("ch");
		FailureRecorder listener(&dispatcher);
		int firstCalls = 0;
		int secondCalls = 0;

		listener.requestRegister(channel, [&firstCalls](const int &) { ++firstCalls; });
		listener.requestRegister(channel, [&secondCalls](const int &) { ++secondCalls; });
		dispatcher.dispatchCommands();

		ASSERT_EQUAL(listener.registerFailures, 1);
		ASSERT_EQUAL(listener.lastStatus, cge::event::DispatchStatus::Duplicate);
		ASSERT_EQUAL(listener.lastChannel, channel.id());

		ASSERT_TRUE(static_cast<bool>(listener.lastRejectedHandler));
		if(listener.lastRejectedHandler)
		{
			cge::event::Event<int> event(1);
			listener.lastRejectedHandler(event);
			ASSERT_EQUAL(firstCalls, 0);
			ASSERT_EQUAL(secondCalls, 1);
		}
	}

	void ListenerUnitTest::unregisterUnknownReported(partest::TestContext &ctx)
	{
		cge::event::EventChannelRegistry registry;
		MockDispatcher dispatcher(&registry);
		dispatcher.setUp();
		const cge::event::EventChannel<int> &channel = registry.getChannel<int>("ch");
		FailureRecorder listener(&dispatcher);

		listener.requestUnregister(channel);
		dispatcher.dispatchCommands();

		ASSERT_EQUAL(listener.unregisterFailures, 1);
		ASSERT_EQUAL(listener.lastStatus, cge::event::DispatchStatus::BadInput);
		ASSERT_EQUAL(listener.lastChannel, channel.id());
	}

	// Requests the dispatcher applies successfully report nothing.
	void ListenerUnitTest::successNotReported(partest::TestContext &ctx)
	{
		cge::event::EventChannelRegistry registry;
		MockDispatcher dispatcher(&registry);
		dispatcher.setUp();
		const cge::event::EventChannel<int> &channel = registry.getChannel<int>("ch");
		FailureRecorder listener(&dispatcher);

		listener.requestRegister(channel, [](const int &) {});
		dispatcher.dispatchCommands();
		listener.requestUnregister(channel);
		dispatcher.dispatchCommands();

		ASSERT_EQUAL(listener.registerFailures, 0);
		ASSERT_EQUAL(listener.unregisterFailures, 0);
	}

	// Re-registering a listener that is already live is queued like any other
	// request, and the dispatcher discards it, so the live handler is unaffected.
	void ListenerUnitTest::reregisterAfterDrain(partest::TestContext &ctx)
	{
		cge::event::EventChannelRegistry registry;
		MockDispatcher dispatcher(&registry);
		dispatcher.setUp();
		const cge::event::EventChannel<int> &channel = registry.getChannel<int>("ch");
		cge::event::ListenerBase listener(&dispatcher);
		int firstCalls = 0;
		int secondCalls = 0;

		listener.requestRegister(channel, [&firstCalls](const int &) { ++firstCalls; });
		dispatcher.dispatchCommands();

		ASSERT_EQUAL(listener.requestRegister(channel, [&secondCalls](const int &) { ++secondCalls; }),
			cge::event::DispatchStatus::Pending);
		dispatcher.dispatchCommands();

		cge::event::Event<int> event(1);
		listener.onEvent(channel.id(), event);

		ASSERT_EQUAL(firstCalls, 1);
		ASSERT_EQUAL(secondCalls, 0);
	}

	void ListenerUnitTest::handlerNotLiveYet(partest::TestContext &ctx)
	{
		cge::event::EventChannelRegistry registry;
		MockDispatcher dispatcher(&registry);
		dispatcher.setUp();
		const cge::event::EventChannel<int> &channel = registry.getChannel<int>("ch");
		cge::event::ListenerBase listener(&dispatcher);
		int calls = 0;

		listener.requestRegister(channel, [&calls](const int &) { ++calls; });

		// Requested but not drained, so the handler is not live yet.
		cge::event::Event<int> event(1);
		listener.onEvent(channel.id(), event);

		ASSERT_EQUAL(calls, 0);
	}

	void ListenerUnitTest::invokesHandler(partest::TestContext &ctx)
	{
		cge::event::EventChannelRegistry registry;
		MockDispatcher dispatcher(&registry);
		dispatcher.setUp();
		const cge::event::EventChannel<int> &channel = registry.getChannel<int>("ch");
		cge::event::ListenerBase listener(&dispatcher);
		int calls = 0;

		listener.requestRegister(channel, [&calls](const int &) { ++calls; });
		dispatcher.dispatchCommands();

		cge::event::Event<int> event(1);
		listener.onEvent(channel.id(), event);

		ASSERT_EQUAL(calls, 1);
	}

	void ListenerUnitTest::passesPayload(partest::TestContext &ctx)
	{
		cge::event::EventChannelRegistry registry;
		MockDispatcher dispatcher(&registry);
		dispatcher.setUp();
		const cge::event::EventChannel<int> &channel = registry.getChannel<int>("ch");
		cge::event::ListenerBase listener(&dispatcher);
		int seen = 0;

		listener.requestRegister(channel, [&seen](const int &v) { seen = v; });
		dispatcher.dispatchCommands();

		cge::event::Event<int> event(77);
		listener.onEvent(channel.id(), event);

		ASSERT_EQUAL(seen, 77);
	}

	void ListenerUnitTest::selectsByChannel(partest::TestContext &ctx)
	{
		cge::event::EventChannelRegistry registry;
		MockDispatcher dispatcher(&registry);
		dispatcher.setUp();
		const cge::event::EventChannel<int> &first = registry.getChannel<int>("first");
		const cge::event::EventChannel<int> &second = registry.getChannel<int>("second");
		cge::event::ListenerBase listener(&dispatcher);
		int firstCalls = 0;
		int secondCalls = 0;

		listener.requestRegister(first, [&firstCalls](const int &) { ++firstCalls; });
		listener.requestRegister(second, [&secondCalls](const int &) { ++secondCalls; });
		dispatcher.dispatchCommands();

		cge::event::Event<int> event(1);
		listener.onEvent(second.id(), event);

		ASSERT_EQUAL(firstCalls, 0);
		ASSERT_EQUAL(secondCalls, 1);
	}

	// One listener on two channels carrying different payload types. Handler
	// selection is already covered by SelectsByChannel, but both its channels
	// carry int, so a mis-keyed lookup there shows up as a wrong count. Here the
	// same mistake casts an Event<string> through a handler expecting an int,
	// which is the failure the channel-to-type binding exists to prevent.
	void ListenerUnitTest::selectsAcrossTypes(partest::TestContext &ctx)
	{
		cge::event::EventChannelRegistry registry;
		MockDispatcher dispatcher(&registry);
		dispatcher.setUp();
		const cge::event::EventChannel<int> &numbers = registry.getChannel<int>("numbers");
		const cge::event::EventChannel<std::string> &names = registry.getChannel<std::string>("names");
		cge::event::ListenerBase listener(&dispatcher);
		int seenNumber = 0;
		std::string seenName;

		listener.requestRegister(numbers, [&seenNumber](const int &v) { seenNumber = v; });
		listener.requestRegister(names, [&seenName](const std::string &v) { seenName = v; });
		dispatcher.dispatchCommands();

		cge::event::Event<std::string> nameEvent(std::string("archer"));
		listener.onEvent(names.id(), nameEvent);

		ASSERT_EQUAL(seenName, std::string("archer"));
		ASSERT_EQUAL(seenNumber, 0);

		cge::event::Event<int> numberEvent(42);
		listener.onEvent(numbers.id(), numberEvent);

		ASSERT_EQUAL(seenNumber, 42);
		ASSERT_EQUAL(seenName, std::string("archer"));
	}

	void ListenerUnitTest::ignoresUnknownChannel(partest::TestContext &ctx)
	{
		cge::event::EventChannelRegistry registry;
		MockDispatcher dispatcher(&registry);
		dispatcher.setUp();
		const cge::event::EventChannel<int> &known = registry.getChannel<int>("known");
		const cge::event::EventChannel<int> &unknown = registry.getChannel<int>("unknown");
		cge::event::ListenerBase listener(&dispatcher);
		int calls = 0;

		listener.requestRegister(known, [&calls](const int &) { ++calls; });
		dispatcher.dispatchCommands();

		cge::event::Event<int> event(1);
		listener.onEvent(unknown.id(), event);

		ASSERT_EQUAL(calls, 0);
	}

	void ListenerUnitTest::memberFunctionForm(partest::TestContext &ctx)
	{
		cge::event::EventChannelRegistry registry;
		MockDispatcher dispatcher(&registry);
		dispatcher.setUp();
		const cge::event::EventChannel<int> &channel = registry.getChannel<int>("ch");
		Recorder recorder(&dispatcher);

		recorder.requestRegister(channel, &recorder, &Recorder::onValue);
		dispatcher.dispatchCommands();

		cge::event::Event<int> event(42);
		recorder.onEvent(channel.id(), event);

		ASSERT_EQUAL(recorder.calls, 1);
		ASSERT_EQUAL(recorder.last, 42);
	}
}
