#include "dispatcher.h"

namespace cge::event
{
	DispatcherBase::DispatcherBase(std::string name, EventChannelRegistry *registry) : SystemBase(name), m_channelRegistry(registry)
	{
		m_registrationChannel = &m_channelRegistry->getChannel<RegistrationRequest>("DispatcherCommand_RegistrationChannel");
		m_unregistrationChannel = &m_channelRegistry->getChannel<RegistrationRequest>("DispatcherCommand_UnregistrationChannel");
	}
	void DispatcherBase::onSetUp()
	{
		// Initialization code for the dispatcher system
	}
	void DispatcherBase::onTearDown()
	{
		// Cleanup code for the dispatcher system
	}

	void DispatcherBase::dispatchEventsUnsafe(std::deque<EventPair> &events)
	{
		while(!events.empty())
		{
			EventPair &event = events.front();
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
			EventPair &command = commands.front();
			commands.pop_front();

			if(command.first == m_registrationChannel->id())
			{
				RegistrationRequest &request = static_cast<Event<RegistrationRequest> *>(command.second.get())->payload;

				registerListener(request.listener, request.channel);
			}
			else if(command.first == m_unregistrationChannel->id())
			{
				RegistrationRequest &request = static_cast<Event<RegistrationRequest> *>(command.second.get())->payload;

				unregisterListener(request.listener, request.channel);
			}
		}
	}

	bool DispatcherBase::pushEvent(const EventChannelBase& channel, const EventBase& event)
	{
		ChannelId channelId = channel.id();
		m_events.push_back(EventPair(channelId, std::make_unique<EventBase>(event)));
		return true;
	}

	bool DispatcherBase::pushCommand(const EventChannelBase& channel, const EventBase& event)
	{
		ChannelId channelId = channel.id();
		m_commands.push_back(EventPair(channelId, std::make_unique<EventBase>(event)));
		return true;
	}

	RegistrationResult DispatcherBase::requestRegisterListener(ListenerBase *listener, const EventChannelBase &channel)
	{
		bool success = pushCommand(*m_registrationChannel, Event<RegistrationRequest>(RegistrationRequest(listener, channel)));
		return success? RegistrationResult::Pending : RegistrationResult::Failure;
	}

	RegistrationResult DispatcherBase::requestUnregisterListener(ListenerBase *listener, const EventChannelBase &channel)
	{
		bool success = pushCommand(*m_unregistrationChannel, Event<RegistrationRequest>(RegistrationRequest(listener, channel)));
		return success? RegistrationResult::Pending : RegistrationResult::Failure;
	}

	void DispatcherBase::registerListener(ListenerBase *listener, const EventChannelBase &channel)
	{
		ChannelId channelId = channel.id();
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

	void DispatcherBase::unregisterListener(ListenerBase *listener, const EventChannelBase &channel)
	{
		auto channelIt = m_listeners.find(channel.id());
		if(channelIt != m_listeners.end())
		{
			auto listenerIt = std::find(channelIt->second.begin(), channelIt->second.end(), listener);
			if(listenerIt != channelIt->second.end())
			{
				// Swap and pop_back to remove the listener efficiently
				*listenerIt = channelIt->second.back();
				channelIt->second.pop_back();
				listener->finalizeRegistration(channel.id(), RegistrationResult::Success);
			}
			else
			{
				listener->finalizeRegistration(channel.id(), RegistrationResult::NotFound);
			}
		}
		else
		{
			listener->finalizeRegistration(channel.id(), RegistrationResult::NotFound);
		}
	}
}