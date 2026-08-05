#include "listener.h"

namespace cge::event
{
	void ListenerBase::onEvent(ChannelId channelId, const EventBase &event)
	{
		HandlerFunction *fn;
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

	void ListenerBase::finalizeRegistration(ChannelId channelId, RegistrationResult result)
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
		case RegistrationResult::Success:
			if(found)
				m_handlers.emplace_back(std::move(handler));
			break;
		case RegistrationResult::Duplicate:
			// Handle duplicate registration if needed
			break;
		case RegistrationResult::Failure:
			// Handle failure if needed
			break;
		default:
			break;
		}
	}

	void ListenerBase::finalizeUnregistration(ChannelId channelId, RegistrationResult result)
	{
		HandlerPairIter it;

		switch(result)
		{
		case RegistrationResult::Success:
			it = std::remove_if(m_handlers.begin(), m_handlers.end(),
				[&channelId](const HandlerPair &pair) {
					return pair.first == channelId;
				});
			if(it != m_handlers.end())
				m_handlers.erase(it, m_handlers.end());
			break;
		case RegistrationResult::NotFound:
			// Handle not found if needed
			break;
		case RegistrationResult::Failure:
			// Handle failure if needed
			break;
		default:
			break;
		}
	}
}