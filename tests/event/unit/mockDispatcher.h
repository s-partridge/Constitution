#ifndef CGE_MOCK_DISPATCHER_H
#define CGE_MOCK_DISPATCHER_H

#include <cstddef>
#include <memory>
#include <utility>

#include "dispatcher.h"
#include "event.h"

namespace cge::test
{
	// A dispatcher with no queueing policy of its own, for unit tests of the
	// things that talk to a dispatcher: broadcasters, commanders and listeners.
	//
	// Pushes are stored in the base class queues rather than a parallel log, so
	// what a test inspects is the same storage the real drain would read. The
	// drains run the base class implementations directly, which means a test can
	// apply a queued registration for real and watch the listener be finalized,
	// with no threading and no swap buffers in the way.
	class MockDispatcher : public cge::event::DispatcherBase
	{
	public:
		explicit MockDispatcher(cge::event::EventChannelRegistry *registry)
			: DispatcherBase("MockDispatcher", registry)
			, m_acceptPushes(true)
		{
		}

		void dispatchEvents() override { dispatchEventsUnsafe(m_events); }
		void dispatchCommands() override { dispatchCommandsUnsafe(m_commands); }

		// Drives the refusal path without needing a lifecycle transition.
		void setAcceptPushes(bool accept) { m_acceptPushes = accept; }

		size_t eventCount() const { return m_events.size(); }
		size_t commandCount() const { return m_commands.size(); }

		// A test indexes these precisely when it believes a push was taken, so an
		// empty queue is what a bug produces rather than what a careless caller
		// produces. Out of range therefore has to report, not crash: a crash
		// costs the assertion that would have named the fault, every later
		// suite's results, and the log they were going into.
		cge::event::ChannelId eventChannel(size_t index) const
		{
			if(index >= m_events.size())
				return cge::event::InvalidChannelId;

			return m_events[index].first;
		}

		cge::event::ChannelId commandChannel(size_t index) const
		{
			if(index >= m_commands.size())
				return cge::event::InvalidChannelId;

			return m_commands[index].first;
		}

		const cge::event::EventBase *queuedEvent(size_t index) const
		{
			if(index >= m_events.size())
				return nullptr;

			return m_events[index].second.get();
		}

		const cge::event::EventBase *queuedCommand(size_t index) const
		{
			if(index >= m_commands.size())
				return nullptr;

			return m_commands[index].second.get();
		}

		// Payload of a queued push, for the common case where the test knows the
		// channel's payload type. By value with a fallback rather than by pointer
		// so the ordinary one-line comparison still reads as one line: an index
		// with nothing behind it fails the caller's assertion on the fallback.
		//
		// The cast is unchecked on purpose. broadcast and command bind payload
		// type to channel type through a single template parameter, so a queued
		// event cannot be carrying a payload of any other type.
		template<typename PayloadType>
		PayloadType eventPayload(size_t index, const PayloadType &fallback = PayloadType()) const
		{
			const cge::event::EventBase *event = queuedEvent(index);
			if(event == nullptr)
				return fallback;

			return static_cast<const cge::event::Event<PayloadType> *>(event)->payload;
		}

		template<typename PayloadType>
		PayloadType commandPayload(size_t index, const PayloadType &fallback = PayloadType()) const
		{
			const cge::event::EventBase *command = queuedCommand(index);
			if(command == nullptr)
				return fallback;

			return static_cast<const cge::event::Event<PayloadType> *>(command)->payload;
		}

	protected:
		event::DispatchStatus onPushEvent(const cge::event::EventChannelBase &channel, std::unique_ptr<cge::event::EventBase> event) override
		{
			if(!m_acceptPushes)
				return event::DispatchStatus::Failure;
			m_events.emplace_back(cge::event::EventPair(channel.id(), std::move(event)));
			return event::DispatchStatus::Pending;
		}

		event::DispatchStatus onPushCommand(const cge::event::EventChannelBase &channel, std::unique_ptr<cge::event::EventBase> event) override
		{
			if(!m_acceptPushes)
				return event::DispatchStatus::Failure;

			m_commands.emplace_back(cge::event::EventPair(channel.id(), std::move(event)));
			return event::DispatchStatus::Pending;
		}

	private:
		bool m_acceptPushes;
	};
}

#endif // CGE_MOCK_DISPATCHER_H
