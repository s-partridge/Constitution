#ifndef CGE_WINDOWSYSTEM_H
#define CGE_WINDOWSYSTEM_H

#include "system.h"
#include "event.h"
#include "engine.h"
#include "broadcaster.h"


// Forward declarations of GLFW and Vulkan instances and functions
struct GLFWwindow;
struct VkInstance_T;
namespace cge::vulkan
{
	struct InstanceFunctions;
}

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
		VkInstance_T *m_vkInstance = nullptr;
		vulkan::InstanceFunctions *m_vulkan = nullptr;
		int m_windowWidth = 1280;
		int m_windowHeight = 720;
	};
}
#endif // CGE_WINDOWSYSTEM_H