#ifndef CGE_DISPATCHER_H
#define CGE_DISPATCHER_H

#include <unordered_map>
#include <functional>
#include <memory>
#include <queue>
#include <vector>

#include "system.h"
#include "event.h"
#include "eventTypes.h"

namespace cge::event
{
	class ListenerBase;

	// TODO: Move these somewhere else, maybe to a new namespace. They're specific to listeners, but Dispatcher requires them for the registration command channels.
	using HandlerFunction = std::function<void(const EventBase &)>;
	using HandlerPair = std::pair<ChannelId, HandlerFunction>;
	using HandlerPairIter = std::vector<HandlerPair>::iterator;

	using ChannelId = size_t;
	using ListenerList = std::vector<ListenerBase *>;
	using ListenerIter = ListenerList::iterator;
	using ChannelMap = std::unordered_map<ChannelId, ListenerList>;
	using ChannelMapIter = ChannelMap::iterator;
	
	using EventPair = std::pair<ChannelId, std::unique_ptr<EventBase>>;
	
	namespace CommandChannel
	{
		constexpr const char *RegisterListener = "DC_RegisterListener";
		constexpr const char *UnregisterListener = "DC_UnregisterListener";
	}

	class DispatcherBase : public SystemBase
	{

	public:
		DispatcherBase(std::string name, EventChannelRegistry *registry);
		virtual ~DispatcherBase();

		void onSetUp() override;
		void onTearDown() override;

		virtual void dispatchEvents() = 0;
		virtual void dispatchCommands() = 0;

	protected:

		// Validate the existence of a command channel
		bool isValidCommand(ChannelId id);

		// Dispatch events and commands without any thread safety or validation checks.
		void dispatchEventsUnsafe(std::deque<EventPair> &events);
		void dispatchCommandsUnsafe(std::deque<EventPair> &commands);

		// Subclass owns enqueue policy (locking, immediate dispatch, wake, etc.).
		virtual DispatchStatus onPushEvent(const EventChannelBase &channel, std::unique_ptr<EventBase> event) = 0;
		virtual DispatchStatus onPushCommand(const EventChannelBase &channel, std::unique_ptr<EventBase> event) = 0;

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
			HandlerPair handlerPair;
			RegistrationRequest(ListenerBase *listenerBase, HandlerPair &&handlerPair) : listener(listenerBase), handlerPair(std::move(handlerPair)) {}
		};

		struct UnregistrationRequest
		{
			ListenerBase *listener;
			ChannelId channelId;
			UnregistrationRequest(ListenerBase *listenerBase, ChannelId channel) : listener(listenerBase), channelId(channel) {}
		};

	protected:
		const EventChannel<RegistrationRequest> *m_registrationChannel;
		const EventChannel<RegistrationRequest> *m_unregistrationChannel;

	private:
		std::unordered_map<ChannelId, ListenerList> m_listeners;

		EventChannelRegistry *m_channelRegistry;
		std::vector<ChannelId> m_validCommandChannels;

		// Thin entry points: cross-cutting hook site, then subclass policy.
		DispatchStatus pushEvent(const EventChannelBase &channel, std::unique_ptr<EventBase> event);
		DispatchStatus pushCommand(const EventChannelBase &channel, std::unique_ptr<EventBase> event);

		DispatchStatus requestRegisterListener(ListenerBase *listener, const EventChannelBase &channel, HandlerPair &&handlerPair);
		DispatchStatus requestUnregisterListener(ListenerBase *listener, const EventChannelBase &channel);
		void registerListener(ListenerBase *listener, HandlerPair &&handlerPair);
		void unregisterListener(ListenerBase *listener, ChannelId channelId);
	};
}

#endif