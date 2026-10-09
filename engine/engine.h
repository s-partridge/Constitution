#ifndef CGE_ENGINE_H
#define CGE_ENGINE_H

#include <vector>
#include <chrono>

#include "clock.h"
#include "system.h"
#include "asyncDispatcher.h"

namespace cge
{
	class Engine
	{
	public:
		// Insert a system into the engine's system list. The engine takes ownership of the system and will manage its lifecycle.
		// Returns true if the system was successfully added, false otherwise.
		// TODO: Nothing currently fails to add a system. Duplicates should be rejected.
		bool addSystem(SystemBase *system);

		static Engine& Instance() { static Engine instance; return instance; }

		void setUp();
		void run();
		void tearDown();

		void update(std::chrono::steady_clock::duration dt);
		void physicsUpdate(std::chrono::steady_clock::duration dt);
		void render();

		constexpr void setMaxPhysicsSubsteps(unsigned maxSubsteps);

	private:
		Engine();
		~Engine();

		cge::Clock m_clock;
		unsigned m_maxPhysicsSubsteps = 32;
		bool m_running = false;

		std::vector<SystemBase *> m_systems;

		cge::event::EventChannelRegistry m_eventRegistry;
		// Non-owning pointer to the AsyncDispatcher system. Systems are owned and managed by m_systems.
		// The dispatcher is unique because it is guaranteed to exist and required for communcation between other systems.
		cge::event::AsyncDispatcher *m_dispatcher;

	};
}

#endif