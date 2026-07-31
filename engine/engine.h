#ifndef CGE_ENGINE_H
#define CGE_ENGINE_H

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