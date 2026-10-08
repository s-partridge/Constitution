#include "listener.h"

namespace cge::event
{
	void ListenerBase::onEvent(ChannelId channelId, const EventBase &event)
	{
		m_inFlightEvents++;
		for(size_t idx = 0; idx < m_pendingRegistrations.size(); ++idx)
		{
			if(m_pendingRegistrations[idx].first == channelId)
			{
				m_pendingRegistrations[idx].second(event);
				break;
			}
		}
		m_inFlightEvents--;

		if(m_inFlightEvents == 0)
			resolvePendingChanges();
	}

	void ListenerBase::registerChannel(HandlerPair &&handlerPair)
	{
		if(m_inFlightEvents > 0)
		{
			m_pendingRegistrations.push_back(std::move(handlerPair));
		}
		else
		{
			m_handlers.push_back(std::move(handlerPair));
		}
	}

	void ListenerBase::unregisterChannel(ChannelId channelId)
	{
		//  Check pending first, don't use auto
		for(size_t idx = 0; idx < m_pendingRegistrations.size(); ++idx)
		{
			if(m_pendingRegistrations[idx].first == channelId)
			{
				m_pendingRegistrations.erase(m_pendingRegistrations.begin() + idx);
				return;
			}
		}

		if(m_inFlightEvents > 0)
		{
			m_pendingUnregistrations.push_back(channelId);
		}
		else
		{
			// Don't use auto
			for(size_t idx = 0; idx < m_handlers.size(); ++idx)
			{
				if(m_handlers[idx].first == channelId)
				{
					m_handlers.erase(m_handlers.begin() + idx);
					return;
				}
			}

		}
	}

	bool ListenerBase::isRegistered(ChannelId channelId) const
	{
		// Check pending registrations first, then check active handlers
		bool found = !m_pendingRegistrations.empty() && std::any_of(m_pendingRegistrations.begin(), m_pendingRegistrations.end(),
			[&channelId](const HandlerPair &pair) {
				return pair.first == channelId;
		});

		return found || std::any_of(m_handlers.begin(), m_handlers.end(),
			[&channelId](const HandlerPair &pair) {
				return pair.first == channelId;
			});
	}

	void ListenerBase::resolvePendingChanges()
	{
		if(!m_pendingRegistrations.empty())
		{
			for(size_t idx = 0; idx < m_pendingRegistrations.size(); ++idx)
			{
				m_handlers.push_back(std::move(m_pendingRegistrations[idx]));
			}
			m_pendingRegistrations.clear();
		}

		if(!m_pendingUnregistrations.empty())
		{
			for(size_t idx = 0; idx < m_pendingUnregistrations.size(); ++idx)
			{
				unregisterChannel(m_pendingUnregistrations[idx]);
			}
			m_pendingUnregistrations.clear();
		}
	}
}