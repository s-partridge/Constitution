#include "clock.h"

namespace cge
{
	Clock::Clock()
	{
		registerChannel(TickTypes::Update, "Update", std::chrono::steady_clock::duration::zero());
		registerChannel(TickTypes::Physics, "Physics", std::chrono::steady_clock::duration::zero());
	}

	bool Clock::registerChannel(const TickType &type, const std::string &name, TickTime::Duration interval)
	{
		for(size_t idx = 0; idx < m_channels.size(); ++idx)
		{
			// Update the existing channel if it already exists. This allows for changing the name or interval of a channel after it has been registered.
			if(m_channels[idx].type.id() == type.id())
			{
				m_channels[idx].name = name;
				m_channels[idx].interval = interval;
				return false;
			}
		}
		
		m_channels.push_back(TickChannel(type, name, interval));
		return true;
	}

	TickTime::Duration Clock::rawTick(const TickType &type)
	{
		TickChannel &channel = getChannel(type);

		std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
		TickTime::Duration delta = now - channel.lastAccess;

		channel.lastAccess = now;
		++channel.count;

		return delta;
	}

	TickTime::Duration Clock::tick(const TickType &type)
	{
		TickTime::Duration raw = rawTick(type);
		double scale = getChannel(type).timeScale;

		return std::chrono::duration_cast<TickTime::Duration>(std::chrono::duration<double>(raw) * scale);
	}

	TickChannel &Clock::getChannel(const TickType &type)
	{
		size_t idx = getChannelIndex(type);
		// Ensure that channel acquisition never fails. If the channel doesn't exist, create it with a default name and zero interval.
		if(idx >= m_channels.size())
		{
			registerChannel(type, "Tick " + std::to_string(type.id()), std::chrono::steady_clock::duration::zero());
			return m_channels.back();
		}
		return m_channels[idx];
	}

	// Return the index of the channel with the given type, or m_channels.size() if not found.
	size_t Clock::getChannelIndex(const TickType &type) const
	{
		size_t idx = 0;
		for(; idx < m_channels.size(); ++idx)
		{
			if(m_channels[idx].type.id() == type.id())
				break;
		}
		return idx;
	}

	TickTime::Duration Clock::getInterval(const TickType &type) const
	{
		size_t idx = getChannelIndex(type);
		if(idx < m_channels.size())
			return m_channels[idx].interval;
		return TickTime::Zero;
	}

	void Clock::setInterval(const TickType &type, TickTime::Duration interval)
	{
		getChannel(type).interval = interval;
	}

	double Clock::getTimeScale(const TickType &type) const
	{
		size_t idx = getChannelIndex(type);
		if(idx < m_channels.size())
			return m_channels[idx].timeScale;
		return 1.0;
	}

	void Clock::setTimeScale(const TickType &type, double timeScale)
	{
		getChannel(type).timeScale = timeScale;
	}

	std::string Clock::getChannelName(const TickType &type) const
	{
		size_t idx = getChannelIndex(type);
		if(idx < m_channels.size())
			return m_channels[idx].name;
		return "Tick " + std::to_string(type.id());
	}

	size_t Clock::getTickCount(const TickType &type) const
	{
		size_t idx = getChannelIndex(type);
		if(idx < m_channels.size())
			return m_channels[idx].count;
		return 0;
	}

	std::chrono::steady_clock::duration Clock::tickUpdate()
	{
		return tick(TickTypes::Update);
	}

	std::chrono::steady_clock::duration Clock::tickPhysics()
	{
		return tick(TickTypes::Physics);
	}

	std::chrono::steady_clock::duration Clock::rawTickUpdate()
	{
		return rawTick(TickTypes::Update);
	}

	std::chrono::steady_clock::duration Clock::rawTickPhysics()
	{
		return rawTick(TickTypes::Physics);
	}

	TickChannel &Clock::getUpdateChannel()
	{
		return getChannel(TickTypes::Update);
	}

	TickChannel &Clock::getPhysicsChannel()
	{
		return getChannel(TickTypes::Physics);
	}

	void Clock::resetDurations()
	{
		TickTime::Instant now = std::chrono::steady_clock::now();

		for(size_t idx = 0; idx < m_channels.size(); ++idx)
		{
			m_channels[idx].lastAccess = now;
		}
	}
}
