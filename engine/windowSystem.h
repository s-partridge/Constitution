#ifndef CGE_WINDOWSYSTEM_H
#define CGE_WINDOWSYSTEM_H

#include "system.h"
#include "event.h"
#include "engine.h"
#include "broadcaster.h"

struct GLFWwindow;

namespace cge
{
	class WindowSystem : public SystemBase
	{
	public:
		WindowSystem(const std::string &name = "WindowSystem", const std::string &windowTitle = "Constitution Game Engine");
		virtual ~WindowSystem() = default;
		void onSetUp() override;
		void update(std::chrono::steady_clock::duration dt) override;
		void render() override;
		void onTearDown() override;

	private:
		cge::event::BroadcasterBase m_windowSystemBroadcaster;
		std::string m_windowTitle;
		const event::EventChannel<bool> *m_shutdownChannel;

		GLFWwindow *m_window = nullptr;
		int m_windowWidth = 1280;
		int m_windowHeight = 720;
	};
}
#endif // CGE_WINDOWSYSTEM_H