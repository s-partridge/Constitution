#ifndef CGE_CLOCK_H
#define CGE_CLOCK_H

#include <atomic>
#include <chrono>
#include <string>
#include <vector>

namespace cge
{
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
}

template<>
struct std::hash<cge::TickType>
{
	size_t operator()(const cge::TickType &type) const noexcept { return std::hash<unsigned>()(type.id()); }
};

namespace cge
{
	namespace TickTime
	{
		using Duration = std::chrono::steady_clock::duration;
		using Instant = std::chrono::steady_clock::time_point;

		inline constexpr Duration Zero = std::chrono::steady_clock::duration::zero();
	}

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
		size_t count;
		TickTime::Instant lastAccess;
		TickTime::Duration interval; // preferred spacing; zero = uncapped
		double timeScale; // per-channel slow-motion / fast-forward multiplier

		TickChannel(const TickType &type, const std::string &name, TickTime::Duration interval)
			: type(type), name(name), count(0), lastAccess(std::chrono::steady_clock::now()), interval(interval), timeScale(1.0)
		{ }
	};

	class Clock
	{
	public:
		Clock(); // registers the Update and Physics channels

		bool registerChannel(const TickType &type, const std::string &name, TickTime::Duration interval); // false if a channel for this type already exists

		TickTime::Duration tick(const TickType &type);       // scaled by the channel's timeScale
		TickTime::Duration rawTick(const TickType &type);    // unscaled, real elapsed time

		TickTime::Duration getInterval(const TickType &type) const;
		void setInterval(const TickType &type, TickTime::Duration interval);
		
		double getTimeScale(const TickType &type) const;
		void setTimeScale(const TickType &type, double timeScale);

		std::string getChannelName(const TickType &type) const;
		size_t getTickCount(const TickType &type) const;

		// Dedicated accessors for the two universal, always-present channels
		TickTime::Duration tickUpdate();
		TickTime::Duration tickPhysics();
		TickTime::Duration rawTickUpdate();
		TickTime::Duration rawTickPhysics();

		void resetDurations(); // resets the lastAccess time for all channels to now, effectively zeroing out the elapsed time

	private:
		std::vector<TickChannel> m_channels;

		size_t getChannelIndex(const TickType &type) const;

		TickChannel &getChannel(const TickType &type);
		TickChannel &getUpdateChannel();
		TickChannel &getPhysicsChannel();
	};
}

#endif // CGE_CLOCK_H