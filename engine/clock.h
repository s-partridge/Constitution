#ifndef CGE_CLOCK_H
#define CGE_CLOCK_H

#include <atomic>
#include <chrono>
#include <string>
#include <unordered_map>

class TickType
{
	const unsigned m_id;

	static unsigned nextId() noexcept
	{
		static std::atomic<unsigned> idCount(0);
		return idCount.fetch_add(1, std::memory_order_relaxed);
	}

public:
	explicit TickType() noexcept : m_id(nextId()) {}
	TickType(const TickType &other) noexcept : m_id(other.m_id) {}

	unsigned id() const noexcept { return m_id; }

	bool operator==(const TickType &rhs) const noexcept { return m_id == rhs.m_id; }
};

template<>
struct std::hash<TickType>
{
	size_t operator()(const TickType &type) const noexcept { return std::hash<unsigned>()(type.id()); }
};

namespace TickTypes
{
	namespace detail
	{
		inline const TickType &getUpdate() noexcept { static const TickType update; return update; }
		inline const TickType &getPhysics() noexcept { static const TickType physics; return physics; }
	}

	static const TickType Update = detail::getUpdate();
	static const TickType Physics = detail::getPhysics();
}

struct TickChannel
{
	TickType type; // redundant copy of the owning key, sanity check only
	std::string name;
	size_t count = 0;
	std::chrono::steady_clock::time_point lastAccess;
	std::chrono::steady_clock::duration interval; // preferred spacing; zero = uncapped
	double timeScale = 1.0; // per-channel slow-motion / fast-forward multiplier
};

class Clock
{
public:
	Clock(); // registers the Update and Physics channels

	bool registerChannel(const TickType &type, const std::string &name, std::chrono::steady_clock::duration interval); // false if a channel for this type already exists

	std::chrono::steady_clock::duration tick(const TickType &type);       // scaled by the channel's timeScale
	std::chrono::steady_clock::duration rawTick(const TickType &type);    // unscaled, real elapsed time
	const TickChannel &getChannel(const TickType &type) const;
	void setInterval(const TickType &type, std::chrono::steady_clock::duration interval);
	void setTimeScale(const TickType &type, double timeScale);

	// Dedicated accessors for the two universal, always-present channels
	std::chrono::steady_clock::duration tickUpdate();
	std::chrono::steady_clock::duration tickPhysics();
	std::chrono::steady_clock::duration rawTickUpdate();
	std::chrono::steady_clock::duration rawTickPhysics();
	const TickChannel &getUpdateChannel() const;
	const TickChannel &getPhysicsChannel() const;

private:
	std::unordered_map<TickType, TickChannel> m_channels;
};

#endif
