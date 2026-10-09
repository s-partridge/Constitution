//

#include "engine.h"

namespace cge
{
	Engine::Engine()
	{
		m_dispatcher = new cge::event::AsyncDispatcher("EngineDispatcher", &m_eventRegistry);
		addSystem(m_dispatcher);

		addComponent(new Engine::EngineListener(*this, m_dispatcher));
	}

	Engine::~Engine()
	{
		// Delete in reverse order to avoid dependency issues during teardown
		for(size_t idx = m_systems.size(); idx > 0; --idx)
		{
			delete m_systems[idx - 1];
		}

		for(size_t idx = m_components.size(); idx > 0; --idx)
		{
			delete m_components[idx - 1];
		}
	}

	bool Engine::addSystem(SystemBase *system)
	{
		m_systems.push_back(system);
		return true;
	}

	bool Engine::addComponent(ComponentBase *component)
	{
		m_components.push_back(component);
		return true;
	}

	void Engine::setUp()
	{
		for(size_t idx = 0; idx < m_systems.size(); ++idx)
		{
			m_systems[idx]->setUp();
		}

		for(size_t idx = 0; idx < m_components.size(); ++idx)
		{
			m_components[idx]->setUp();
		}
	}

	void Engine::run()
	{
		setUp();

		m_running = true;

		// Reset the clock to ensure the first tick is accurate and not a large delta from setup time.
		m_clock.resetDurations();
		m_clock.setInterval(TickTypes::Physics, std::chrono::milliseconds(16)); // 60Hz physics update

		while(m_running)
		{
			TickTime::Duration dt = m_clock.tickUpdate();
			//Handle user input once the system exists

			// Should become part of earlyUpdate loop
			m_dispatcher->dispatchCommands();
			update(dt);

			//Should become part of lateUpdate loop
			m_dispatcher->dispatchCommands();

			physicsUpdate(m_clock.tickPhysics());
			//physics update with fixed timestep
			//fixedUpdate(pdt);

			// Get current frame delta

			render();
		}

		tearDown();
	}

	void Engine::update(TickTime::Duration dt)
	{
		for(size_t idx = 0; idx < m_systems.size(); ++idx)
		{
			m_systems[idx]->update(dt);
		}
	}

	void Engine::physicsUpdate(TickTime::Duration dt)
	{
		TickTime::Duration physicsInterval = m_clock.getInterval(TickTypes::Physics);
		TickTime::Duration remainingTime = dt;

		if(physicsInterval == TickTime::Zero)
		{
			// If the physics interval is zero, we treat it as uncapped and process the entire dt in one step.
			for(size_t idx = 0; idx < m_systems.size(); ++idx)
			{
				m_systems[idx]->physicsUpdate(remainingTime);
			}
			return;
		}

		for (unsigned substep = 0; substep < m_maxPhysicsSubsteps - 1; ++substep)
		{
			TickTime::Duration step = std::min(remainingTime, physicsInterval);
			for(size_t idx = 0; idx < m_systems.size(); ++idx)
			{
				m_systems[idx]->physicsUpdate(step);
			}
			remainingTime -= step;
		}

		if(remainingTime > TickTime::Zero)
		{
			for(size_t idx = 0; idx < m_systems.size(); ++idx)
			{
				m_systems[idx]->physicsUpdate(remainingTime);
			}
		}
	}

	constexpr void Engine::setMaxPhysicsSubsteps(unsigned maxSubsteps)
	{
		if(maxSubsteps > 0)
			m_maxPhysicsSubsteps = maxSubsteps;
		else
			m_maxPhysicsSubsteps = 1; // Ensure at least one substep
	}

	void Engine::tearDown()
	{
		// Tear down in reverse order of setup to avoid dependency issues
		for(size_t idx = m_components.size(); idx > 0; --idx)
		{
			 m_components[idx - 1]->tearDown();
		}

		// From last to second, skipping first system (the dispatcher).
		for(size_t idx = m_systems.size() - 1; idx > 0; --idx)
		{
			m_systems[idx]->tearDown();
		}

		// The dispatcher is always the first system added, so it will be the last to tear down. Ensure all commands are processed before shutdown.
		m_dispatcher->dispatchCommands();
		m_dispatcher->tearDown();
	}

	Engine::EngineListener::EngineListener(Engine &engine, cge::event::DispatcherBase *dispatcher) : m_engine(engine), ListenerBase(dispatcher)
	{
		// TODO: Currently every event requires a payload of some kind. This may not always be necessary, but for the moment bool is used as a workaround here.
		// Possible solution: could use a std::monostate or a custom empty struct as the payload type for events that don't need to carry data.
		// This still requires callers to have a useless parameter, though, so it might be worth overloading listener to allow a parameter-free callback where the payload is ignored.
		 m_shutdownChannel = &m_engine.m_eventRegistry.getChannel<bool>(EngineEvent::EngineShutdown);
	}

	void Engine::EngineListener::setUp()
	{
		requestRegister(*m_shutdownChannel, &m_engine, &Engine::stopRunning);
	}

	void Engine::EngineListener::tearDown()
	{
		requestUnregister(*m_shutdownChannel);
	}
}