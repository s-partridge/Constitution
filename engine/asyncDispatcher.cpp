#include "asyncDispatcher.h"

namespace cge::event
{
	void AsyncDispatcher::dispatchEvents()
	{
		m_eventReentryCount = 0;
		while(true)
		{
			{
				std::lock_guard<std::mutex>lock(m_eventQueueMutex);
				if(m_events.empty())
					break;
				// Move the events out and into the swap buffer. New events will be written to the real queue while these process.
				std::swap(m_events, m_eventSwap);
			}

			dispatchEventsUnsafe(m_eventSwap);
			++m_eventReentryCount;
		}
	}

	void AsyncDispatcher::dispatchCommands()
	{
		m_commandReentryCount = 0;
		while(true)
		{
			{
				std::lock_guard<std::mutex>lock(m_commandQueueMutex);
				if(m_commands.empty())
					break;
				// Move the commands out and into the swap buffer. New events will be written to the real queue while these process.
				std::swap(m_commands, m_commandSwap);
			}

			dispatchEventsUnsafe(m_commandSwap);
			++m_commandReentryCount;
		}
	}

	bool AsyncDispatcher::onPushEvent(const EventChannelBase &channel, std::unique_ptr<EventBase> event)
	{
		std::lock_guard<std::mutex>lock(m_eventQueueMutex);
		m_events.emplace_back(channel.id(), std::move(event));
		return true;
	}
	bool AsyncDispatcher::onPushCommand(const EventChannelBase &channel, std::unique_ptr<EventBase> command)
	{
		std::lock_guard<std::mutex>lock(m_commandQueueMutex);
		m_commands.emplace_back(channel.id(), std::move(command));
		return true;
	}
}