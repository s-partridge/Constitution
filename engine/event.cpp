#include "event.h"

namespace cge::event
{
	EventChannelRegistry::~EventChannelRegistry()
	{
		destroyMap();
	}

	EventChannelRegistry &EventChannelRegistry::operator=(EventChannelRegistry &&other) noexcept
	{
		{
			if(this != &other)
			{
				destroyMap();
				m_channels = std::move(other.m_channels);
				// Probably not necessary, but just to be safe, clear the other registry's channels
				other.m_channels.clear();
			}
			return *this;
		}
	}

	void EventChannelRegistry::destroyMap() noexcept
	{
		for(std::unordered_map<ChannelTag, EventChannelBase *>::iterator pair = m_channels.begin(); pair != m_channels.end(); ++pair)
		{
			delete pair->second;
		}
		m_channels.clear();
	}
}