#ifndef CGE_ENGINE_H
#define CGE_ENGINE_H

#include <vector>
#include <chrono>

#include "clock.h"
#include "system.h"
#include "event.h"
#include "asyncDispatcher.h"
#include "listener.h"

namespace cge
{
	namespace EngineEvent
	{
		constexpr const char *EngineShutdown = "CGE_Shutdown";
	}

	class Engine
	{
	public:
		// Insert a system into the engine's system list. The engine takes ownership of the system and will manage its lifecycle.
		// Returns true if the system was successfully added, false otherwise.
		// TODO: Nothing currently fails to add a system. Duplicates should be rejected.
		bool addSystem(SystemBase *system);
		bool addComponent(ComponentBase *component);

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
		std::vector<ComponentBase *> m_components;

		cge::event::EventChannelRegistry m_eventRegistry;
		// Non-owning pointer to the AsyncDispatcher system. Systems are owned and managed by m_systems.
		// The dispatcher is unique because it is guaranteed to exist and required for communcation between other systems.
		cge::event::AsyncDispatcher *m_dispatcher;

		
		void stopRunning(bool expected) noexcept { m_running = false; }

		class EngineListener : public cge::event::ListenerBase
		{
			const event::EventChannel<bool> *m_shutdownChannel;
			Engine &m_engine;
		public:
			EngineListener(Engine &engine, cge::event::DispatcherBase *dispatcher);
			~EngineListener() = default;
			
			void setUp() override;
			void tearDown() override;
		};
	};
}

#endif