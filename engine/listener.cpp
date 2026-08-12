#include "listener.h"

namespace cge::event
{
	void ListenerBase::onEvent(ChannelId channelId, const EventBase &event)
	{
		HandlerPairIter it = std::find_if(
			m_handlers.begin(), m_handlers.end(),
			[&channelId](const HandlerPair &pair) {
				return pair.first == channelId;
			});
		if(it != m_handlers.end())
		{
			it->second(event);
		}
	}

	void ListenerBase::finalizeRegistration(ChannelId channelId, DispatchStatus result)
	{
		HandlerPairIter it;
		HandlerFunction handler;

		bool found = false;
			
		{
			std::lock_guard<std::mutex> lock(m_pendingMutex);
			it = std::find_if(
				m_pendingHandlers.begin(), m_pendingHandlers.end(),
				[&channelId](const HandlerPair &pair) {
					return pair.first == channelId;
				});
			if(it != m_pendingHandlers.end())
			{
				handler = std::move(it->second);
				m_pendingHandlers.erase(it);
				found = true;
			}
		}
		switch(result)
		{
		case DispatchStatus::Success:
			if(found)
				m_handlers.emplace_back(channelId, std::move(handler));
			break;
		case DispatchStatus::Duplicate:
			// Handle duplicate registration if needed
			break;
		case DispatchStatus::Failure:
			// Handle failure if needed
			break;
		default:
			break;
		}
	}

	void ListenerBase::finalizeUnregistration(ChannelId channelId, DispatchStatus result)
	{
		HandlerPairIter it;

		switch(result)
		{
		case DispatchStatus::Success:
			it = std::remove_if(m_handlers.begin(), m_handlers.end(),
				[&channelId](const HandlerPair &pair) {
					return pair.first == channelId;
				});
			if(it != m_handlers.end())
				m_handlers.erase(it, m_handlers.end());
			break;
		case DispatchStatus::BadInput:
			// Handle not found if needed
			break;
		case DispatchStatus::Failure:
			// Handle failure if needed
			break;
		default:
			break;
		}
	}
}