#include "scaffold.h"

namespace cge
{
	void Scaffold::build(Engine &engine)
	{
		initializeSystems(engine);
		initializeGame(engine);
		configureGame(engine);
	}
}