#include "gameScaffold.h"
#include "windowSystem.h"

namespace cge
{
	void GameScaffold::initializeSystems(Engine &engine)
	{
		engine.addSystem(new WindowSystem());
	}

	void GameScaffold::initializeGame(Engine &engine)
	{
	}

	void GameScaffold::configureGame(Engine &engine)
	{
	}
}