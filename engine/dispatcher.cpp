#include "dispatcher.h"

namespace cge::event
{
	Dispatcher::Dispatcher(EventChannelRegistry *registry) : SystemBase("Dispatcher"), m_channelRegistry(registry)
	{
		m_registrationChannel = &m_channelRegistry->getChannel<RegistrationRequest>("DispatcherCommand_RegistrationChannel");
		m_unregistrationChannel = &m_channelRegistry->getChannel<RegistrationRequest>("DispatcherCommand_UnregistrationChannel");
	}
	void Dispatcher::onSetUp()
	{
		// Initialization code for the dispatcher system
	}
	void Dispatcher::onTearDown()
	{
		// Cleanup code for the dispatcher system
	}
	bool Dispatcher::pushEvent(const EventChannelBase& channel, const EventBase& event)
	{
		ChannelId channelId = channel.id();
		m_events.push_back(EventPair(channelId, std::make_unique<EventBase>(event)));
		return true;
	}

	bool Dispatcher::pushCommand(const EventChannelBase& channel, const EventBase& event)
	{
		ChannelId channelId = channel.id();
		m_commands.push_back(EventPair(channelId, std::make_unique<EventBase>(event)));
		return true;
	}

	RegistrationResult Dispatcher::requestRegisterListener(ListenerBase *listener, const EventChannelBase &channel)
	{
		pushCommand(*m_registrationChannel, Event<RegistrationRequest>(RegistrationRequest(listener, channel)));
		return RegistrationResult::Pending;
	}

	RegistrationResult Dispatcher::requestUnregisterListener(ListenerBase *listener, const EventChannelBase &channel)
	{
		pushCommand(*m_unregistrationChannel, Event<RegistrationRequest>(RegistrationRequest(listener, channel)));
		return RegistrationResult::Pending;
	}

	void Dispatcher::registerListener(ListenerBase *listener, const EventChannelBase &channel)
	{
		ChannelId channelId = channel.id();
		// check whether the listener is already registered for this channel
		bool contains = std::find(m_listeners[channelId].begin(), m_listeners[channelId].end(), listener) != m_listeners[channelId].end();

		if(!contains)
		{
			m_listeners[channelId].push_back(listener);
		}
	}

	void Dispatcher::unregisterListener(ListenerBase *listener, const EventChannelBase &channel)
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
			}
		}
		else
		{
		}
	}
}