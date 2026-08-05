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

		template<typename PayloadType>
		void broadcast(const EventChannel<PayloadType> &channel, const PayloadType payload)
		{
			std::unique_ptr<EventBase> event = std::make_unique<Event<PayloadType>>(payload);
			m_dispatcher->pushEvent(channel, std::move(event));
		}
	private:
		DispatcherBase *m_dispatcher;
	};

	class CommanderBase
	{
	public:
		CommanderBase(DispatcherBase *dispatcher) : m_dispatcher(dispatcher) {}
		virtual ~CommanderBase() = default;

		template<typename PayloadType>
		void command(const EventChannel<PayloadType> &channel, const PayloadType payload)
		{
			std::unique_ptr<EventBase> event = std::make_unique<Event<PayloadType>>(payload);
			m_dispatcher->pushCommand(channel, std::move(event));
		}
	private:
		DispatcherBase *m_dispatcher;
	};
}

#endif