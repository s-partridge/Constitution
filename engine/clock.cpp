#include "clock.h"

Clock::Clock()
{
	registerChannel(TickTypes::Update, "Update", std::chrono::steady_clock::duration::zero());
	registerChannel(TickTypes::Physics, "Physics", std::chrono::steady_clock::duration::zero());
}

bool Clock::registerChannel(const TickType &type, const std::string &name, std::chrono::steady_clock::duration interval)
{
	if(m_channels.find(type) != m_channels.end())
		return false;

	m_channels.emplace(type, TickChannel(type, name, interval));
	return true;
}

std::chrono::steady_clock::duration Clock::rawTick(const TickType &type)
{
	TickChannel &channel = m_channels.at(type);

	std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
	std::chrono::steady_clock::duration delta = now - channel.lastAccess;

	channel.lastAccess = now;
	++channel.count;

	return delta;
}

std::chrono::steady_clock::duration Clock::tick(const TickType &type)
{
	std::chrono::steady_clock::duration raw = rawTick(type);
	double scale = m_channels.at(type).timeScale;

	return std::chrono::duration_cast<std::chrono::steady_clock::duration>(std::chrono::duration<double>(raw) * scale);
}

const TickChannel &Clock::getChannel(const TickType &type) const
{
	return m_channels.at(type);
}

void Clock::setInterval(const TickType &type, std::chrono::steady_clock::duration interval)
{
	m_channels.at(type).interval = interval;
}

void Clock::setTimeScale(const TickType &type, double timeScale)
{
	m_channels.at(type).timeScale = timeScale;
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

const TickChannel &Clock::getUpdateChannel() const
{
	return getChannel(TickTypes::Update);
}

const TickChannel &Clock::getPhysicsChannel() const
{
	return getChannel(TickTypes::Physics);
}
