#include "dispatcher.h"
#include "listener.h"

#include <algorithm>
//#include <iostream>

namespace cge::event
{
	DispatcherBase::DispatcherBase(std::string name, EventChannelRegistry *registry)
		: SystemBase(name), m_channelRegistry(registry),
		m_eventReentryCount(0), m_commandReentryCount(0),
		m_active(false)
	{
		m_registrationChannel = &m_channelRegistry->getChannel<RegistrationRequest>(CommandChannel::RegisterListener);
		m_unregistrationChannel = &m_channelRegistry->getChannel<RegistrationRequest>(CommandChannel::UnregisterListener);

		m_validCommandChannels.push_back(m_registrationChannel->id());
		m_validCommandChannels.push_back(m_unregistrationChannel->id());
	}

	DispatcherBase::~DispatcherBase()
	{
		// TODO: Replace with an error log once the logging system is in place.
		// Using cerr is way too noisy. This can't be left uncommented for now.
		/*
		for(auto &listenerList : m_listeners)
		{
			if(listenerList.second.size() > 0)
			{
				std::cerr << "Warning: Dispatcher was destroyed with " << listenerList.second.size() << " listeners still registered for channel " << listenerList.first << "." << std::endl;
			}
		}
		if(m_events.size() > 0)
		{
			std::cerr << "Warning: Dispatcher was destroyed with " << m_events.size() << " events still queued." << std::endl;
		}
		if(m_commands.size() > 0)
		{
			std::cerr << "Warning: Dispatcher was destroyed with " << m_commands.size() << " commands still queued." << std::endl;
		}*/
	}

	void DispatcherBase::onSetUp()
	{
		m_active = true;
	}

	void DispatcherBase::onTearDown()
	{
		m_active = false;
	}

	bool DispatcherBase::isValidCommand(ChannelId id)
	{
		for(size_t idx = 0; idx < m_validCommandChannels.size(); ++idx)
		{
			if(m_validCommandChannels[idx] == id)
				return true;
		}
		return false;
	}

	void DispatcherBase::dispatchEventsUnsafe(std::deque<EventPair> &events)
	{
		while(!events.empty())
		{
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

				registerListener(request.listener, std::move(request.handlerPair));
			}
			else if(command.first == m_unregistrationChannel->id())
			{
				UnregistrationRequest &request = static_cast<Event<UnregistrationRequest> *>(command.second.get())->payload;

				unregisterListener(request.listener, request.channelId);
			}
		}
	}

	DispatchStatus DispatcherBase::pushEvent(const EventChannelBase &channel, std::unique_ptr<EventBase> event)
	{
		return onPushEvent(channel, std::move(event));
	}

	DispatchStatus DispatcherBase::pushCommand(const EventChannelBase &channel, std::unique_ptr<EventBase> command)
	{
		if(!isValidCommand(channel.id()))
			return DispatchStatus::Invalid;

		return onPushCommand(channel, std::move(command));
	}

	DispatchStatus DispatcherBase::requestRegisterListener(ListenerBase *listener, const EventChannelBase &channel, HandlerPair &&handlerPair)
	{
		std::unique_ptr<EventBase> event =
			std::make_unique<Event<RegistrationRequest>>(RegistrationRequest(listener, std::move(handlerPair)));
		return pushCommand(*m_registrationChannel, std::move(event));
	}

	DispatchStatus DispatcherBase::requestUnregisterListener(ListenerBase *listener, const EventChannelBase &channel)
	{
		std::unique_ptr<EventBase> event =
			std::make_unique<Event<UnregistrationRequest>>(UnregistrationRequest(listener, channel.id()));
		return pushCommand(*m_unregistrationChannel, std::move(event));
	}

	void DispatcherBase::registerListener(ListenerBase *listener, HandlerPair &&handlerPair)
	{
		// check whether the listener is already registered for this channel
		bool contains = std::find(m_listeners[handlerPair.first].begin(), m_listeners[handlerPair.first].end(), listener) != m_listeners[handlerPair.first].end();

		if(!contains)
		{
			m_listeners[handlerPair.first].push_back(listener);
			listener->registerChannel(std::move(handlerPair));
		}
		else
		{
			// The listener is already registered for this channel
			listener->onRegisterFailed(std::move(handlerPair), DispatchStatus::Duplicate);
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
				listener->unregisterChannel(channelId);
			}
			else
			{
				// No listener found to remove for this channel
				listener->onUnregisterFailed(channelId, DispatchStatus::BadInput);
			}
		}
		// Channel doesn't have any listeners at all
		else
		{
			listener->onUnregisterFailed(channelId, DispatchStatus::BadInput);
		}
	}
}