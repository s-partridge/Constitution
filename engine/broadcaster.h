#ifndef CGE_BROADCASTER_H
#define CGE_BROADCASTER_H

#include "dispatcher.h"
#include "event.h"

namespace cge::event
{
	class BroadcasterBase
	{
	public:
		BroadcasterBase(Dispatcher *dispatcher) : m_dispatcher(dispatcher) {}
		virtual ~BroadcasterBase() = default;

		template<typename PayloadType>
		void broadcast(const EventChannel<PayloadType> &channel, const PayloadType payload)
		{
			m_dispatcher->pushEvent(channel, Event<PayloadType>(payload));
		}
	private:
		Dispatcher *m_dispatcher;
	};

	class CommanderBase
	{
	public:
		CommanderBase(Dispatcher *dispatcher) : m_dispatcher(dispatcher) {}
		virtual ~CommanderBase() = default;
		template<typename PayloadType>
		void command(const EventChannel<PayloadType> &channel, const PayloadType payload)
		{
			m_dispatcher->pushCommand(channel, Event<PayloadType>(payload));
		}
	private:
		Dispatcher *m_dispatcher;
	};
}

#endif