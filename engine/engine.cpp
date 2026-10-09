//

#include "engine.h"

namespace cge
{
	Engine::Engine()
	{
		m_dispatcher = new cge::event::AsyncDispatcher("EngineDispatcher", &m_eventRegistry);
		addSystem(m_dispatcher);
	}

	Engine::~Engine()
	{
		// Delete in reverse order to avoid dependency issues during teardown
		for(size_t idx = m_systems.size(); idx > 0; --idx)
		{
			delete m_systems[idx - 1];
		}
	}

	bool Engine::addSystem(SystemBase *system)
	{
		m_systems.push_back(system);
		return true;
	}

	void Engine::setUp()
	{
		for(size_t idx = 0; idx < m_systems.size(); ++idx)
		{
			m_systems[idx]->setUp();
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

			update(dt);

			physicsUpdate(m_clock.tickPhysics());
			//phyiscs update with fixed timestep
			//fixedUpdate(pdt);

			// Get current frame delta

			render();
		}
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
		for(size_t idx = m_systems.size(); idx > 0; --idx)
		{
			m_systems[idx - 1]->tearDown();
		}
	}
}