#include "listenerTest.h"

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
	}

	ListenerUnitTest::ListenerUnitTest()
		: TestBase("ListenerUnitTest", "Unit tests for ListenerBase against a mock dispatcher.")
	{
		partest::TestFlags flags = partest::TEST_FLAGS_INHERIT;

		addTest("ReturnsPending", flags, [this]() { returnsPending(); });
		addTest("QueuesOneCommand", flags, [this]() { queuesOneCommand(); });
		addTest("DuplicateResult", flags, [this]() { duplicateResult(); });
		addTest("DuplicateQueuesNothing", flags, [this]() { duplicateQueuesNothing(); });
		addTest("RefusedResult", flags, [this]() { refusedResult(); });
		addTest("RefusedRetry", flags, [this]() { refusedRetry(); });
		addTest("UnregisterClearsPending", flags, [this]() { unregisterClearsPending(); });
		addTest("UnregisterUnknown", flags, [this]() { unregisterUnknown(); });
		addTest("ReregisterAfterDrain", flags, [this]() { reregisterAfterDrain(); });

		addTest("HandlerNotLiveYet", flags, [this]() { handlerNotLiveYet(); });
		addTest("InvokesHandler", flags, [this]() { invokesHandler(); });
		addTest("PassesPayload", flags, [this]() { passesPayload(); });
		addTest("SelectsByChannel", flags, [this]() { selectsByChannel(); });
		addTest("IgnoresUnknownChannel", flags, [this]() { ignoresUnknownChannel(); });
		addTest("MemberFunctionForm", flags, [this]() { memberFunctionForm(); });
	}

	void ListenerUnitTest::returnsPending()
	{
		cge::event::EventChannelRegistry registry;
		MockDispatcher dispatcher(&registry);
		dispatcher.setUp();
		const cge::event::EventChannel<int> &channel = registry.getChannel<int>("ch");
		cge::event::ListenerBase listener(&dispatcher);

		ASSERT_TRUE(listener.requestRegister(channel, [](const int &) {})
			== cge::event::RegistrationResult::Pending);
	}

	void ListenerUnitTest::queuesOneCommand()
	{
		cge::event::EventChannelRegistry registry;
		MockDispatcher dispatcher(&registry);
		dispatcher.setUp();
		const cge::event::EventChannel<int> &channel = registry.getChannel<int>("ch");
		cge::event::ListenerBase listener(&dispatcher);

		listener.requestRegister(channel, [](const int &) {});

		ASSERT_EQUAL(dispatcher.commandCount(), static_cast<size_t>(1));
	}

	void ListenerUnitTest::duplicateResult()
	{
		cge::event::EventChannelRegistry registry;
		MockDispatcher dispatcher(&registry);
		dispatcher.setUp();
		const cge::event::EventChannel<int> &channel = registry.getChannel<int>("ch");
		cge::event::ListenerBase listener(&dispatcher);

		listener.requestRegister(channel, [](const int &) {});

		ASSERT_TRUE(listener.requestRegister(channel, [](const int &) {})
			== cge::event::RegistrationResult::Duplicate);
	}

	void ListenerUnitTest::duplicateQueuesNothing()
	{
		cge::event::EventChannelRegistry registry;
		MockDispatcher dispatcher(&registry);
		dispatcher.setUp();
		const cge::event::EventChannel<int> &channel = registry.getChannel<int>("ch");
		cge::event::ListenerBase listener(&dispatcher);

		listener.requestRegister(channel, [](const int &) {});
		listener.requestRegister(channel, [](const int &) {});

		ASSERT_EQUAL(dispatcher.commandCount(), static_cast<size_t>(1));
	}

	void ListenerUnitTest::refusedResult()
	{
		cge::event::EventChannelRegistry registry;
		MockDispatcher dispatcher(&registry);
		dispatcher.setUp();
		dispatcher.setAcceptPushes(false);
		const cge::event::EventChannel<int> &channel = registry.getChannel<int>("ch");
		cge::event::ListenerBase listener(&dispatcher);

		ASSERT_TRUE(listener.requestRegister(channel, [](const int &) {})
			== cge::event::RegistrationResult::Failure);
	}

	// A refused registration must leave the listener able to try again. Expected
	// to fail: the handler is committed to the pending list before the dispatcher
	// is asked, so a refusal leaves a stale entry and the retry reads as a
	// duplicate. See docs/open-items.md.
	void ListenerUnitTest::refusedRetry()
	{
		cge::event::EventChannelRegistry registry;
		MockDispatcher dispatcher(&registry);
		dispatcher.setUp();
		const cge::event::EventChannel<int> &channel = registry.getChannel<int>("ch");
		cge::event::ListenerBase listener(&dispatcher);

		dispatcher.setAcceptPushes(false);
		listener.requestRegister(channel, [](const int &) {});
		dispatcher.setAcceptPushes(true);

		ASSERT_TRUE(listener.requestRegister(channel, [](const int &) {})
			== cge::event::RegistrationResult::Pending);
	}

	void ListenerUnitTest::unregisterClearsPending()
	{
		cge::event::EventChannelRegistry registry;
		MockDispatcher dispatcher(&registry);
		dispatcher.setUp();
		const cge::event::EventChannel<int> &channel = registry.getChannel<int>("ch");
		cge::event::ListenerBase listener(&dispatcher);

		listener.requestRegister(channel, [](const int &) {});
		listener.requestUnregister(channel);

		// The pending entry is gone, so this is a fresh request rather than a duplicate.
		ASSERT_TRUE(listener.requestRegister(channel, [](const int &) {})
			== cge::event::RegistrationResult::Pending);
	}

	void ListenerUnitTest::unregisterUnknown()
	{
		cge::event::EventChannelRegistry registry;
		MockDispatcher dispatcher(&registry);
		dispatcher.setUp();
		const cge::event::EventChannel<int> &channel = registry.getChannel<int>("ch");
		cge::event::ListenerBase listener(&dispatcher);

		listener.requestUnregister(channel);

		// What the call returns is unsettled, see D1 in docs/test-refactor.md.
		// What is settled is that it must not poison the listener.
		ASSERT_TRUE(listener.requestRegister(channel, [](const int &) {})
			== cge::event::RegistrationResult::Pending);
	}

	// Re-registering a listener that is already registered is the same caller
	// error as re-registering a pending one. Expected to fail: the pending list
	// is cleared on finalize, so the guard no longer sees anything. See A1 in
	// docs/test-refactor.md.
	void ListenerUnitTest::reregisterAfterDrain()
	{
		cge::event::EventChannelRegistry registry;
		MockDispatcher dispatcher(&registry);
		dispatcher.setUp();
		const cge::event::EventChannel<int> &channel = registry.getChannel<int>("ch");
		cge::event::ListenerBase listener(&dispatcher);

		listener.requestRegister(channel, [](const int &) {});
		dispatcher.dispatchCommands();

		ASSERT_TRUE(listener.requestRegister(channel, [](const int &) {})
			== cge::event::RegistrationResult::Duplicate);
	}

	void ListenerUnitTest::handlerNotLiveYet()
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

	void ListenerUnitTest::invokesHandler()
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

	void ListenerUnitTest::passesPayload()
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

	void ListenerUnitTest::selectsByChannel()
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

	void ListenerUnitTest::ignoresUnknownChannel()
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

	void ListenerUnitTest::memberFunctionForm()
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
