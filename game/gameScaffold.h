#include "scaffold.h"

namespace cge
{
	class GameScaffold : public Scaffold
	{
	public:
		GameScaffold() {}
		void initializeSystems(Engine &engine) override;
		void initializeGame(Engine &engine) override;
		void configureGame(Engine &engine) override;
	};
}

