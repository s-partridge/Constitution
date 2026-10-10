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
struct VkSwapchainKHR_T;

struct VkPhysicalDevice_T;
struct VkDevice_T;
struct VkQueue_T;

struct VkImage_T;
struct VkImageView_T;

namespace cge::vulkan
{
	struct VulkanFunctions;
	struct DeviceFunctions;
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

		// Handle for Vulkan global and instance functions
		vulkan::VulkanFunctions *m_vulkan = nullptr;
		// Handle to the Vulkan instance itself
		VkInstance_T *m_vkInstance = nullptr;

		// Handle to the Vulkan surface. This is used to present rendered images to the window.
		VkSurfaceKHR_T *m_surface = nullptr;
		// Handle to the chosen physical GPU
		VkPhysicalDevice_T *m_physicalDevice = nullptr;

		// Handle to the logical Vulkan device. This is used to submit rendering commands to the GPU.
		VkDevice_T *m_device = nullptr;
		// Handle for Vulkan device functions. These are tied to the logical device and are used to submit commands to the GPU.
		vulkan::DeviceFunctions *m_deviceFunctions = nullptr;
		// Queue used to submit rendering commands to the GPU.
		VkQueue_T *m_queue = nullptr;

		// Handle to the Vulkan swapchain. This is used to manage the images that are presented to the window.
		VkSwapchainKHR_T *m_swapchain = nullptr;

		// Queue family index for the chosen GPU. This is used to submit rendering commands to the GPU.
		// A queue family is a group of queues that can execute commands on the GPU. Different queue families may support different types of operations (graphics, compute, transfer, etc.).
		uint32_t m_activeQueueFamilyIndex = 0;
		// Format of the images in the swapchain. This is used to ensure that the images are compatible with the window surface.
		// The format is specified as a VkFormat enum value, but that can't be forward-declared. VkFormat is a C enum, so we can use a uint32_t to store it instead.
		uint32_t m_swapchainFormat = 0;
		uint32_t m_swapchainWidth = 0;
		uint32_t m_swapchainHeight = 0;

		// Number of images actually created in the swapchain. This is used to manage the images that are presented to the window.
		uint32_t m_swapchainImageCount = 0;
		// Array of handles to the images in the swapchain. The images are managed by the swapchain, but the array is owned by the WindowSystem and must be deleted in tearDown.
		VkImage_T **m_swapchainImages = nullptr;
		// Array of handles to the image views for the images in the swapchain. There is one view per image.
		VkImageView_T **m_swapchainImageViews = nullptr;

		// NOTE:
		// m_vulkan and m_deviceFunctions are created and owned by WindowSystem, and are deleted in tearDown.
		// The indices of m_swapchainImageViews require cleanup in tearDown. m_swapchainImages are managed by the swapchain, and do not require explicit cleanup.
		// The arrays containing m_swapchainImages and m_swapchainImageViews are created in onSetUp, and are deleted in tearDown.
		// m_window, m_vkInstance, m_surface, m_device, and m_swapchain require cleanup in tearDown, but are not deleted.
		// m_queue is created and destroyed along with m_device, and does not require explicit cleanup.
		// m_physicalDevice requires no cleanup; it is managed by the Vulkan loader.
		
		int m_windowWidth = 1280;
		int m_windowHeight = 720;
	};
}
#endif // CGE_WINDOWSYSTEM_H