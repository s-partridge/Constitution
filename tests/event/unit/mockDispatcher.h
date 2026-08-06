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

		cge::event::ChannelId eventChannel(size_t index) const { return m_events[index].first; }
		cge::event::ChannelId commandChannel(size_t index) const { return m_commands[index].first; }

		const cge::event::EventBase &queuedEvent(size_t index) const { return *m_events[index].second; }
		const cge::event::EventBase &queuedCommand(size_t index) const { return *m_commands[index].second; }

		// Payload of a queued push, for the common case where the test knows the
		// channel's payload type.
		template<typename PayloadType>
		const PayloadType &eventPayload(size_t index) const
		{
			return static_cast<const cge::event::Event<PayloadType> &>(queuedEvent(index)).payload;
		}

		template<typename PayloadType>
		const PayloadType &commandPayload(size_t index) const
		{
			return static_cast<const cge::event::Event<PayloadType> &>(queuedCommand(index)).payload;
		}

	protected:
		bool onPushEvent(const cge::event::EventChannelBase &channel, std::unique_ptr<cge::event::EventBase> event) override
		{
			if(!m_acceptPushes)
				return false;

			m_events.push_back(cge::event::EventPair(channel.id(), std::move(event)));
			return true;
		}

		bool onPushCommand(const cge::event::EventChannelBase &channel, std::unique_ptr<cge::event::EventBase> event) override
		{
			if(!m_acceptPushes)
				return false;

			m_commands.push_back(cge::event::EventPair(channel.id(), std::move(event)));
			return true;
		}

	private:
		bool m_acceptPushes;
	};
}

#endif // CGE_MOCK_DISPATCHER_H
