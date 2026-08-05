#ifndef CGE_DISPATCHER_H
#define CGE_DISPATCHER_H

#include <unordered_map>
#include <memory>
#include <queue>
#include <list>

#include "system.h"
#include "event.h"

namespace cge::event
{
	class ListenerBase;

	enum class RegistrationResult
	{
		Success,	// Registration/Unregistration was successful
		Failure,	// Registration/Unregistration failed due to an unknown error
		Duplicate,	// Registration/Unregistration failed because the listener is already registered for the channel
		Pending,	// Registration/Unregistration is pending and will be completed later
		NotFound	// Unregistration failed because the listener was not found for the channel; not used for registration
	};

	using ChannelId = size_t;
	using ListenerList = std::list<ListenerBase *>;
	using ListenerIter = ListenerList::iterator;
	using ChannelMap = std::unordered_map<ChannelId, ListenerList>;
	using ChannelMapIter = ChannelMap::iterator;
	
	using EventPair = std::pair<ChannelId, std::unique_ptr<EventBase>>;
	
	class DispatcherBase : public SystemBase
	{

	public:
		DispatcherBase(std::string name, EventChannelRegistry *registry);
		virtual ~DispatcherBase() = default;

		void onSetUp() override;
		void onTearDown() override;

		virtual void dispatchEvents() = 0;
		virtual void dispatchCommands() = 0;

	protected:
		// Dispatch events and commands without any thread safety or validation checks.
		void dispatchEventsUnsafe(std::deque<EventPair> &events);
		void dispatchCommandsUnsafe(std::deque<EventPair> &commands);

		// Subclass owns enqueue policy (locking, immediate dispatch, wake, etc.).
		virtual bool onPushEvent(const EventChannelBase &channel, std::unique_ptr<EventBase> event) = 0;
		virtual bool onPushCommand(const EventChannelBase &channel, std::unique_ptr<EventBase> event) = 0;

		// Queues are subclass-accessible for lock/steal/swap under their own policy.
		// Callers that need a handoff use a temp container and std::swap directly.
		std::deque<EventPair> m_events;
		std::deque<EventPair> m_commands;

		unsigned m_eventReentryCount;
		unsigned m_commandReentryCount;

		// true while the dispatcher is set up and may accept / process work.
		SystemStatus m_active;

	private:
		friend class BroadcasterBase;
		friend class CommanderBase;
		friend class ListenerBase;

		struct RegistrationRequest
		{
			ListenerBase *listener;
			ChannelId channelId;
			RegistrationRequest(ListenerBase *listenerBase, ChannelId channelId) : listener(listenerBase), channelId(channelId) {}
		};

		std::unordered_map<ChannelId, ListenerList> m_listeners;

		EventChannelRegistry *m_channelRegistry;

		const EventChannel<RegistrationRequest> *m_registrationChannel;
		const EventChannel<RegistrationRequest> *m_unregistrationChannel;

		// Thin entry points: cross-cutting hook site, then subclass policy.
		bool pushEvent(const EventChannelBase &channel, std::unique_ptr<EventBase> event);
		bool pushCommand(const EventChannelBase &channel, std::unique_ptr<EventBase> event);

		RegistrationResult requestRegisterListener(ListenerBase *listener, const EventChannelBase &channel);
		RegistrationResult requestUnregisterListener(ListenerBase *listener, const EventChannelBase &channel);
		void registerListener(ListenerBase *listener, ChannelId channelId);
		void unregisterListener(ListenerBase *listener, ChannelId channelId);
	};
}

#endif