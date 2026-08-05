#include "dispatcher.h"
#include "listener.h"

#include <algorithm>

namespace cge::event
{
	DispatcherBase::DispatcherBase(std::string name, EventChannelRegistry *registry)
		: SystemBase(name), m_channelRegistry(registry),
		m_eventReentryCount(0), m_commandReentryCount(0),
		m_active(false)
	{
		m_registrationChannel = &m_channelRegistry->getChannel<RegistrationRequest>("DispatcherCommand_RegistrationChannel");
		m_unregistrationChannel = &m_channelRegistry->getChannel<RegistrationRequest>("DispatcherCommand_UnregistrationChannel");
	}
	void DispatcherBase::onSetUp()
	{
		m_active = true;
	}
	void DispatcherBase::onTearDown()
	{
		m_active = false;
	}

	void DispatcherBase::dispatchEventsUnsafe(std::deque<EventPair> &events)
	{
		while(!events.empty())
		{
			// Move out before pop_front — a reference to front() is invalidated by pop.
			EventPair event = std::move(events.front());
			events.pop_front();

			ChannelId channelId = event.first;
			ChannelMapIter channelIt = m_listeners.find(channelId);
			if(channelIt != m_listeners.end())
			{
				for(ListenerBase *listener : channelIt->second)
				{
					listener->onEvent(channelId, *event.second);
				}
			}
		}
	}
	
	void DispatcherBase::dispatchCommandsUnsafe(std::deque<EventPair> &commands)
	{
		while(!commands.empty())
		{
			EventPair command = std::move(commands.front());
			commands.pop_front();

			if(command.first == m_registrationChannel->id())
			{
				RegistrationRequest &request = static_cast<Event<RegistrationRequest> *>(command.second.get())->payload;

				registerListener(request.listener, request.channelId);
			}
			else if(command.first == m_unregistrationChannel->id())
			{
				RegistrationRequest &request = static_cast<Event<RegistrationRequest> *>(command.second.get())->payload;

				unregisterListener(request.listener, request.channelId);
			}
		}
	}

	bool DispatcherBase::pushEvent(const EventChannelBase &channel, std::unique_ptr<EventBase> event)
	{
		return onPushEvent(channel, std::move(event));
	}

	bool DispatcherBase::pushCommand(const EventChannelBase &channel, std::unique_ptr<EventBase> command)
	{
		return onPushCommand(channel, std::move(command));
	}

	RegistrationResult DispatcherBase::requestRegisterListener(ListenerBase *listener, const EventChannelBase &channel)
	{
		std::unique_ptr<EventBase> event =
			std::make_unique<Event<RegistrationRequest>>(RegistrationRequest(listener, channel.id()));
		bool success = pushCommand(*m_registrationChannel, std::move(event));
		return success ? RegistrationResult::Pending : RegistrationResult::Failure;
	}

	RegistrationResult DispatcherBase::requestUnregisterListener(ListenerBase *listener, const EventChannelBase &channel)
	{
		std::unique_ptr<EventBase> event =
			std::make_unique<Event<RegistrationRequest>>(RegistrationRequest(listener, channel.id()));
		bool success = pushCommand(*m_unregistrationChannel, std::move(event));
		return success ? RegistrationResult::Pending : RegistrationResult::Failure;
	}

	void DispatcherBase::registerListener(ListenerBase *listener, ChannelId channelId)
	{
		// check whether the listener is already registered for this channel
		bool contains = std::find(m_listeners[channelId].begin(), m_listeners[channelId].end(), listener) != m_listeners[channelId].end();

		if(!contains)
		{
			m_listeners[channelId].push_back(listener);
			listener->finalizeRegistration(channelId, RegistrationResult::Success);
		}
		else
		{
			listener->finalizeRegistration(channelId, RegistrationResult::Duplicate);
		}
	}

	void DispatcherBase::unregisterListener(ListenerBase *listener, ChannelId channelId)
	{
		auto channelIt = m_listeners.find(channelId);
		if(channelIt != m_listeners.end())
		{
			auto listenerIt = std::find(channelIt->second.begin(), channelIt->second.end(), listener);
			if(listenerIt != channelIt->second.end())
			{
				// Swap and pop_back to remove the listener efficiently
				*listenerIt = channelIt->second.back();
				channelIt->second.pop_back();
				listener->finalizeUnregistration(channelId, RegistrationResult::Success);
			}
			else
			{
				listener->finalizeUnregistration(channelId, RegistrationResult::NotFound);
			}
		}
		else
		{
			listener->finalizeUnregistration(channelId, RegistrationResult::NotFound);
		}
	}
}