#ifndef ENGINE_H
#define ENGINE_H

namespace cge
{
	class Engine
	{
	private:
		Engine() {}

		static Engine& Instance() { static Engine instance; return instance; }
	};
}

#endif