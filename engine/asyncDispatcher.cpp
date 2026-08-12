#include "asyncDispatcher.h"

namespace cge::event
{
	void AsyncDispatcher::onSetUp()
	{
		// Hold both queue mutexes so no push can check m_active and enqueue mid-startup
		std::lock_guard<std::mutex> eventLock(m_eventQueueMutex);
		std::lock_guard<std::mutex> commandLock(m_commandQueueMutex);
		m_active = true;
	}
	void AsyncDispatcher::onTearDown()
	{
		// Hold both queue mutexes so no push can check m_active and enqueue mid-shutdown
		std::lock_guard<std::mutex> eventLock(m_eventQueueMutex);
		std::lock_guard<std::mutex> commandLock(m_commandQueueMutex);
		m_active = false;
	}

	void AsyncDispatcher::dispatchEvents()
	{
		m_eventReentryCount = 0;
		while(true)
		{
			{
				std::lock_guard<std::mutex> lock(m_eventQueueMutex);
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
				std::lock_guard<std::mutex> lock(m_commandQueueMutex);
				if(m_commands.empty())
					break;
				// Move the commands out and into the swap buffer. New commands will be written to the real queue while these process.
				std::swap(m_commands, m_commandSwap);
			}

			dispatchCommandsUnsafe(m_commandSwap);
			++m_commandReentryCount;
		}
	}

	DispatchStatus AsyncDispatcher::onPushEvent(const EventChannelBase &channel, std::unique_ptr<EventBase> event)
	{
		std::lock_guard<std::mutex> lock(m_eventQueueMutex);
		if(!m_active)
			return DispatchStatus::NotReady;

		m_events.emplace_back(channel.id(), std::move(event));
		return DispatchStatus::Pending;
	}

	DispatchStatus AsyncDispatcher::onPushCommand(const EventChannelBase &channel, std::unique_ptr<EventBase> command)
	{
		std::lock_guard<std::mutex> lock(m_commandQueueMutex);
		// Only unregistration requests can only be pushed while dispatcher is inactive.
		if(!m_active && channel.id() != m_unregistrationChannel->id())
			return DispatchStatus::NotReady;

		m_commands.emplace_back(channel.id(), std::move(command));
		return DispatchStatus::Pending;
	}
}