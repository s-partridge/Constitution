#ifndef CGE_DISPATCHER_H
#define CGE_DISPATCHER_H

#include <unordered_map>
#include <memory>
#include <queue>
#include <list>

#include "system.h"
#include "listener.h"
#include "event.h"

namespace cge::event
{
	enum class RegistrationResult
	{
		Success,	// Registration/Unregistration was successful
		Failure,	// Registration/Unregistration failed due to an unknown error
		Duplicate,	// Registration/Unregistration failed because the listener is already registered for the channel
		Pending,	// Registration/Unregistration is pending and will be completed later
		NotFound	// Unregistration failed because the listener was not found for the channel; not used for registration
	};

	using ChannelId = size_t;
	class Dispatcher : public SystemBase
	{
		using ListenerList = std::list<ListenerBase *>;
		using EventPair = std::pair<ChannelId, std::unique_ptr<EventBase>>;
		std::unordered_map<ChannelId, ListenerList> m_listeners;

		std::deque<EventPair> m_events;
		std::deque<EventPair> m_commands;
	public:
		Dispatcher(EventChannelRegistry *registry);
		virtual ~Dispatcher() = default;

		void onSetUp() override;
		void onTearDown() override;

	private:
		friend class BroadcasterBase;
		friend class CommanderBase;
		friend class ListenerBase;

		struct RegistrationRequest
		{
			ListenerBase *listener;
			const EventChannelBase &channel;
			RegistrationRequest(ListenerBase *listenerBase, const EventChannelBase &channel) : listener(listenerBase), channel(channel) {}
		};

		EventChannelRegistry *m_channelRegistry;

		EventChannel<RegistrationRequest> *m_registrationChannel;
		EventChannel<RegistrationRequest> *m_unregistrationChannel;

		bool pushEvent(const EventChannelBase& channel, const EventBase& event);
		bool pushCommand(const EventChannelBase& channel, const EventBase& event);

		RegistrationResult requestRegisterListener(ListenerBase *listener, const EventChannelBase &channel);
		RegistrationResult requestUnregisterListener(ListenerBase *listener, const EventChannelBase &channel);
		void registerListener(ListenerBase *listener, const EventChannelBase &channel);
		void unregisterListener(ListenerBase *listener, const EventChannelBase &channel);
	};
}

#endif