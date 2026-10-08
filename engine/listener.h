#ifndef CGE_LISTENER_H
#define CGE_LISTENER_H

#include <functional>
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
		ListenerBase(DispatcherBase *dispatcher) : m_dispatcher(dispatcher), m_inFlightEvents(0) {}
		virtual ~ListenerBase() = default;

		template <typename PayloadType, std::invocable<const PayloadType &> CallbackType>
		DispatchStatus requestRegister(const EventChannel<PayloadType> &channel, CallbackType callback)
		{
			ChannelId id = channel.id();

			// Wrap the callback in a lambda that takes an EventBase reference, casts it to the correct type, and invokes the callback with the payload.
			HandlerPair handlerPair(id, [callback](const EventBase &event) {
				const Event<PayloadType> &typedEvent = static_cast<const Event<PayloadType> &>(event);
				std::invoke(callback, typedEvent.payload);
			});
			
			// Pass the handler pair to the dispatcher for registration.
			DispatchStatus status = m_dispatcher->requestRegisterListener(this, channel, std::move(handlerPair));
			return status;
		}

		// Version of subscribe that requires channel, source object, and raw function pointer
		template <typename PayloadType, typename SourceType, std::invocable<SourceType*, const PayloadType&> CallbackType>
		DispatchStatus requestRegister(const EventChannel<PayloadType> &channel, SourceType *self, CallbackType callback)
		{
			ChannelId id = channel.id();
			
			// Wrap the callback in a lambda that captures the source object and invokes the member function.
			HandlerPair handlerPair(id, [self, callback](const EventBase &event) {
				const Event<PayloadType> &typedEvent = static_cast<const Event<PayloadType> &>(event);
				std::invoke(callback, self, typedEvent.payload);
			});

			// Pass the handler pair to the dispatcher for registration.
			DispatchStatus status = m_dispatcher->requestRegisterListener(this, channel, std::move(handlerPair));
			return status;
		}

		template<typename PayloadType>
		DispatchStatus requestUnregister(const EventChannel<PayloadType>& channel)
		{
			// Pass the request to the dispatcher to unregister this listener from the specified channel.
			return m_dispatcher->requestUnregisterListener(this, channel);
		}

		void onEvent(ChannelId channelId, const EventBase &event);

	protected:
		virtual void onRegisterFailed(HandlerPair &&handlerPair, DispatchStatus status) {}
		virtual void onUnregisterFailed(ChannelId channelId, DispatchStatus status) {}

	private:
		friend class DispatcherBase;

		std::vector<HandlerPair> m_handlers;
		
		// This mechanism protects handlers from reentrant calls during event dispatch.
		std::vector<HandlerPair> m_pendingRegistrations;
		std::vector<ChannelId> m_pendingUnregistrations;
		size_t m_inFlightEvents;

		DispatcherBase *m_dispatcher;

		// Called by the dispatcher when a registration request is processed.
		void registerChannel(HandlerPair &&handlerPair);
		// Called by the dispatcher when an unregistration request is processed.
		void unregisterChannel(ChannelId channelId);

		// Check if the listener is already registered for a given channel, returns true whether it is pending or fully registered.
		bool isRegistered(ChannelId channelId) const;
		// Apply any pending registration changes. This is called after all in-flight events have been processed to ensure that the listener's state is consistent.
		void resolvePendingChanges();
	};
}

#endif // CGE_LISTENER_H