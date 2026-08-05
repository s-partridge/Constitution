#ifndef CGE_BROADCASTER_H
#define CGE_BROADCASTER_H

#include "dispatcher.h"
#include "event.h"

namespace cge::event
{
	class BroadcasterBase
	{
	public:
		BroadcasterBase(DispatcherBase *dispatcher) : m_dispatcher(dispatcher) {}
		virtual ~BroadcasterBase() = default;

		// Returns false when the dispatcher refused the push (inactive); the event
		// is discarded in that case, not queued for later.
		template<typename PayloadType>
		bool broadcast(const EventChannel<PayloadType> &channel, const PayloadType &payload)
		{
			std::unique_ptr<EventBase> event = std::make_unique<Event<PayloadType>>(payload);
			return m_dispatcher->pushEvent(channel, std::move(event));
		}
	private:
		DispatcherBase *m_dispatcher;
	};

	class CommanderBase
	{
	public:
		CommanderBase(DispatcherBase *dispatcher) : m_dispatcher(dispatcher) {}
		virtual ~CommanderBase() = default;

		// Returns false when the dispatcher refused the push (inactive); the
		// command is discarded in that case, not queued for later.
		template<typename PayloadType>
		bool command(const EventChannel<PayloadType> &channel, const PayloadType &payload)
		{
			std::unique_ptr<EventBase> event = std::make_unique<Event<PayloadType>>(payload);
			return m_dispatcher->pushCommand(channel, std::move(event));
		}
	private:
		DispatcherBase *m_dispatcher;
	};
}

#endif