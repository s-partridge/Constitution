#include "engine.h"
#include "gameScaffold.h"

int main(int argc, char **argv)
{
	cge::Engine &engine = cge::Engine::Instance();

	// Create the game scaffold and initialize the engine with it
	cge::GameScaffold scaffold;
	scaffold.build(engine);

	engine.run();

	return 0;
}