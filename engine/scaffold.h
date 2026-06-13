#ifndef SCAFFOLD_H
#define SCAFFOLD_H

#include "engine.h"

namespace cge
{
	class Scaffold
	{
	public:
		Scaffold() {}

		virtual ~Scaffold() {}

		void build(Engine &engine);

		virtual void initializeSystems(Engine &engine) = 0;
		virtual void initializeGame(Engine &engine) = 0;
		virtual void configureGame(Engine &engine) = 0;
	};
}

#endif