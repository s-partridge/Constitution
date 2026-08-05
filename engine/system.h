#ifndef CGE_SYSTEM_H
#define CGE_SYSTEM_H

#include <atomic>
#include <chrono>
#include <string>

namespace cge
{
	// Placeholder until a richer status enum/type is needed.
	using SystemStatus = bool;

	class SystemBase
	{
	public:
		SystemBase(const std::string& name) : m_id(nextId()) {}
		virtual ~SystemBase() = default;

		size_t id() const noexcept { return m_id; }

		virtual void physicsUpdate(std::chrono::steady_clock::duration dt) {}
		virtual void update(std::chrono::steady_clock::duration dt) {}
		virtual void render() {}

		void setUp()
		{
			onSetUp();
		}

		void tearDown()
		{
			onTearDown();
		}

	protected:
		virtual void onSetUp() = 0;
		virtual void onTearDown() = 0;

	private:
		size_t m_id;
		std::string m_name;

		static size_t nextId()
		{
			static std::atomic<size_t> id(0);
			return id.fetch_add(1, std::memory_order_relaxed);
		}
	};
}

#endif