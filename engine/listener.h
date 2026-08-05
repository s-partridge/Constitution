#ifndef CGE_LISTENER_H
#define CGE_LISTENER_H

#include <functional>
#include <mutex>

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

		ListenerBase(DispatcherBase *dispatcher) : m_dispatcher(dispatcher) {}
		virtual ~ListenerBase() = default;

		template <typename PayloadType, std::invocable<const PayloadType &> CallbackType>
		RegistrationResult requestRegister(const EventChannel<PayloadType> &channel, CallbackType callback)
		{
			ChannelId id = channel.id();
			{
				std::lock_guard<std::mutex> lock(m_pendingMutex);
				// Check pending handlers to see if this channel is already waiting for registration
				HandlerPairIter it = std::find_if(
					m_pendingHandlers.begin(), m_pendingHandlers.end(),
					[&id](const HandlerPair &pair) {
						return pair.first == id;
					});

				if(it != m_pendingHandlers.end())
				{
					return RegistrationResult::Duplicate;
				}

				HandlerPair handlerPair(id, [callback](const EventBase &event) {
					const Event<PayloadType> &typedEvent = static_cast<const Event<PayloadType> &>(event);
					std::invoke(callback, typedEvent.payload);
				});
				m_pendingHandlers.push_back(std::move(handlerPair));
			}
			return m_dispatcher->requestRegisterListener(this, channel);
		}

		// Version of subscribe that requires channel, source object, and raw function pointer
		template <typename PayloadType, typename SourceType, std::invocable<SourceType*, const PayloadType&> CallbackType>
		RegistrationResult requestRegister(const EventChannel<PayloadType> &channel, SourceType *self, CallbackType callback)
		{
			ChannelId id = channel.id();
			{
				std::lock_guard<std::mutex> lock(m_pendingMutex);
				// Check pending handlers to see if this channel is already waiting for registration
				HandlerPairIter it = std::find_if(
					m_pendingHandlers.begin(), m_pendingHandlers.end(),
					[&id](const HandlerPair &pair) {
						return pair.first == id;
					});
				if(it != m_pendingHandlers.end())
				{
					return RegistrationResult::Duplicate;
				}
				HandlerPair handlerPair(id, [self, callback](const EventBase &event) {
					const Event<PayloadType> &typedEvent = static_cast<const Event<PayloadType> &>(event);
					std::invoke(callback, self, typedEvent.payload);
				});
				m_pendingHandlers.push_back(std::move(handlerPair));
			}
			return m_dispatcher->requestRegisterListener(this, channel);
		}

		template<typename PayloadType>
		RegistrationResult requestUnregister(const EventChannel<PayloadType>& channel)
		{
			return m_dispatcher->requestUnregisterListener(this, channel);
		}

		void onEvent(ChannelId channelId, const EventBase &event);

	private:
		friend class DispatcherBase;

		std::vector<HandlerPair> m_handlers;
		std::vector<HandlerPair> m_pendingHandlers;
		std::mutex m_pendingMutex;

		DispatcherBase *m_dispatcher;

		void finalizeRegistration(ChannelId channelId, RegistrationResult result);
		void finalizeUnregistration(ChannelId channelId, RegistrationResult result);
	};
}

#endif // CGE_LISTENER_H