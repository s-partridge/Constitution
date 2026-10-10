#ifndef CGE_WINDOWSYSTEM_H
#define CGE_WINDOWSYSTEM_H

#include "system.h"
#include "event.h"
#include "engine.h"
#include "broadcaster.h"


// Types for GLFW and Vulkan instances and functions
struct GLFWwindow;
struct VkInstance_T;
struct VkSurfaceKHR_T;
struct VkPhysicalDevice_T;

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

		// Handle to the GLFW window
		GLFWwindow *m_window = nullptr;

		// Handle for Vulkan instance functions
		vulkan::InstanceFunctions *m_vulkan = nullptr;
		// Handle to the Vulkan instance itself
		VkInstance_T *m_vkInstance = nullptr;
		// Handle to the Vulkan surface. This is used to present rendered images to the window.
		VkSurfaceKHR_T *m_surface = nullptr;
		// Handle to the chosen GPU for rendering
		VkPhysicalDevice_T *m_activeDevice = nullptr;

		// Queue family index for the chosen GPU. This is used to submit rendering commands to the GPU.
		// A queue family is a group of queues that can execute commands on the GPU. Different queue families may support different types of operations (graphics, compute, transfer, etc.).
		uint32_t m_activeQueueFamilyIndex = 0;

		// NOTE:
		// m_vulkan is created and owned by WindowSystem, and is deleted in tearDown.
		// m_vkInstance and m_surface require cleanup in tearDown, but are not deleted.
		// m_activeDevice requires no cleanup; it is managed by the Vulkan loader.
		
		int m_windowWidth = 1280;
		int m_windowHeight = 720;
	};
}
#endif // CGE_WINDOWSYSTEM_H