#ifndef CGE_LISTENER_H
#define CGE_LISTENER_H

#include <functional>
#include <mutex>
#include <algorithm>
#include <vector>

#include "dispatcher.h"
#include "event.h"

namespace cge::event
{
	enum class ListenerState
	{
		Unregistered,
		PendingRegistration,
		Registered,
		PendingUnregistration
	};

	class ListenerBase
	{
	public:
		using HandlerFunction = std::function<void(const EventBase &)>;
		using HandlerPair = std::pair<ChannelId, HandlerFunction>;
		using HandlerPairIter = std::vector<HandlerPair>::iterator;

		ListenerBase(DispatcherBase *dispatcher) : m_dispatcher(dispatcher), m_inFlightEvents(0) {}
		virtual ~ListenerBase() = default;

		template <typename PayloadType, std::invocable<const PayloadType &> CallbackType>
		DispatchStatus requestRegister(const EventChannel<PayloadType> &channel, CallbackType callback)
		{
			ChannelId id = channel.id();

			// Don't re-register on the same channel.
			if(isRegistered(id))
				return DispatchStatus::Duplicate;

			// Wrap the callback in a lambda that takes an EventBase reference, casts it to the correct type, and invokes the callback with the payload.
			HandlerPair handlerPair(id, [callback](const EventBase &event) {
				const Event<PayloadType> &typedEvent = static_cast<const Event<PayloadType> &>(event);
				std::invoke(callback, typedEvent.payload);
			});
			
			DispatchStatus status = m_dispatcher->requestRegisterListener(this, channel);

			if(status == DispatchStatus::Success || status == DispatchStatus::Pending)
				registerChannel(std::move(handlerPair));

			return status;
		}

		// Version of subscribe that requires channel, source object, and raw function pointer
		template <typename PayloadType, typename SourceType, std::invocable<SourceType*, const PayloadType&> CallbackType>
		DispatchStatus requestRegister(const EventChannel<PayloadType> &channel, SourceType *self, CallbackType callback)
		{
			ChannelId id = channel.id();

			// Don't re-register on the same channel.
			if(isRegistered(id))
				return DispatchStatus::Duplicate;
			
			// Wrap the callback in a lambda that captures the source object and invokes the member function.
			HandlerPair handlerPair(id, [self, callback](const EventBase &event) {
				const Event<PayloadType> &typedEvent = static_cast<const Event<PayloadType> &>(event);
				std::invoke(callback, self, typedEvent.payload);
			});


			DispatchStatus status = m_dispatcher->requestRegisterListener(this, channel);

			if(status == DispatchStatus::Success || status == DispatchStatus::Pending)
				registerChannel(std::move(handlerPair));

			return status;
		}

		template<typename PayloadType>
		DispatchStatus requestUnregister(const EventChannel<PayloadType>& channel)
		{
			unregisterChannel(channel.id());
			return m_dispatcher->requestUnregisterListener(this, channel);
		}

		void onEvent(ChannelId channelId, const EventBase &event);

	private:
		friend class DispatcherBase;

		std::vector<HandlerPair> m_handlers;
		
		std::vector<HandlerPair> m_pendingRegistrations;
		std::vector<ChannelId> m_pendingUnregistrations;

		size_t m_inFlightEvents;

		DispatcherBase *m_dispatcher;

		void registerChannel(HandlerPair &&handlerPair);
		void unregisterChannel(ChannelId channelId);

		bool isRegistered(ChannelId channelId) const;
		void resolvePendingChanges();
	};
}

#endif // CGE_LISTENER_H