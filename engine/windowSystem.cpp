#include "windowSystem.h"
#include "event.h"
#include "engine.h"

#include "vulkanBootstrapper.h"
#define GLFW_INCLUDE_VULKAN	// Using Vulkan instead of OpenGL.
#include <GLFW/glfw3.h>

namespace cge
{
	WindowSystem::WindowSystem(const std::string &name, const std::string &windowTitle)
		: SystemBase(name), m_windowSystemBroadcaster(Engine::Instance().getDispatcher()),
		  m_windowTitle(windowTitle)
	{
		m_shutdownChannel = &Engine::Instance().getEventRegistry().getChannel<bool>(EngineEvent::EngineShutdown);
	}

	void WindowSystem::onSetUp()
	{
		// Initialize the GLFW context and validate that it was successful.
		if(glfwInit() == GLFW_FALSE)
			return; // TODO: Log this once error handling system exists.

		// Set configuration hints for the GLFW window. We don't need an OpenGL context since we'll be using Vulkan.
		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); // No OpenGL context; we'll use Vulkan later.
		// Create the window with the specified width, height, and title. The last two parameters are for monitor and shared context, which we don't need.
		// Monitor is for fullscreen mode, and share is a shared context is for sharing OpenGL contexts.
		m_window = glfwCreateWindow(m_windowWidth, m_windowHeight, m_windowTitle.c_str(), nullptr, nullptr);

		m_vulkan = new vulkan::InstanceFunctions();
		// Get the initial function pointer for vkGetInstanceProcAddr from GLFW. This function is used to load other Vulkan functions.
		PFN_vkGetInstanceProcAddr vulcanLocator = reinterpret_cast<PFN_vkGetInstanceProcAddr>(glfwGetInstanceProcAddress(nullptr, "vkGetInstanceProcAddr"));
		// Pass it to the Vulkan bootstrapper to load the global functions. If it fails, we log and return.
		if(vulcanLocator == nullptr || !vulkan::loadGlobalfunctions(*m_vulkan, vulcanLocator))
			// Nothing to do here; the engine will fail to start if Vulkan is not available. Just log this and return.
			// TODO: Log this once error handling system exists.
			return;

		// Get the required Vulkan instance extensions from GLFW. These are necessary for creating a Vulkan instance that can interface with the window system.
		// TODO: Figure out what these extensions are, why they might be required, and when.
		std::uint32_t instanceExtensionCount = 0;
		const char **extensions = glfwGetRequiredInstanceExtensions(&instanceExtensionCount);
		// A nullptr here would mean that Vulkan is present, but is not able to interface with the window system. This could happen, for example, on a server without a display. In that case, we should log and return.
		// The window system likely shouldn't be loaded at all in those configurations, but we should still handle it gracefully.
		if(extensions == nullptr)
		{
			return; // TODO: Log this once error handling system exists.
		}

		// Create a Vulkan application info structure. This is used to provide information about the application to the Vulkan driver.
		VkApplicationInfo appInfo = VkApplicationInfo();
		appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		appInfo.pApplicationName = m_windowTitle.c_str();
		appInfo.pEngineName = "Constitution";
		appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0); // No idea whether this is useful. TODO: Make this configurable.
		appInfo.apiVersion = VK_API_VERSION_1_4;

		// Create a Vulkan "InstanceCreateInfo" structure. This is used to specify the parameters for creating a Vulkan instance.
		VkInstanceCreateInfo createInfo = VkInstanceCreateInfo();
		createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		createInfo.pApplicationInfo = &appInfo;
		createInfo.enabledExtensionCount = instanceExtensionCount;
		createInfo.ppEnabledExtensionNames = extensions;

#ifdef _DEBUG
		// Enable validation layers in debug builds, if available.
		// VK_LAYER_KHRONOS_validation is only available when the Vulkan SDK is present on the system.
		// Validation layers are used to check for errors and provide debugging information.

		// Get the count of available layers
		std::uint32_t layerCount = 0;
		m_vulkan->vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

		// Run the same enumeration with a non-null pointer to get the actual layer properties. This is used to check if the validation layer is available.
		VkLayerProperties *availableLayers = new VkLayerProperties[layerCount];
		m_vulkan->vkEnumerateInstanceLayerProperties(&layerCount, availableLayers);

		const char *validationLayers[] = { "VK_LAYER_KHRONOS_validation" };
		bool hasValidationLayer = false;

		#pragma warning (push)
		#pragma warning (disable : 6385) // Disable warning about reading out of bounds. The size won't grow between these two calls.
		for(std::uint32_t i = 0; i < layerCount; ++i)
		{
			if(std::strcmp(availableLayers[i].layerName, validationLayers[0]) == 0)
			{
				hasValidationLayer = true;
				break;
			}
		}
		#pragma warning (pop)

		delete[] availableLayers;

		if(hasValidationLayer)
		{
			createInfo.enabledLayerCount = 1;
			createInfo.ppEnabledLayerNames = validationLayers;
		}
		// TODO: Else, record debug-level log that the system does not have the validation layer available.
		// TODO: Add a debug messenger (VK_EXT_debug_utils) once a renderer system exists, to route validation messages to the engine's log.
		// Until then, the validation layer prints its own messages to the console and, on Windows, the debugger output.
#endif
		// Create the Vulkan instance using the configuration specified in the createInfo structure.
		if(m_vulkan->vkCreateInstance(&createInfo, nullptr, &m_vkInstance) != VK_SUCCESS)
		{
			// TODO: Log this once error handling system exists.
			return;
		}

		// Load the instance-level Vulkan functions using the created Vulkan instance. This allows us to use Vulkan functions that require a valid instance.
		loadInstanceFunctions(*m_vulkan, m_vkInstance);

		// Create a Vulkan surface for the GLFW window. This surface is used to present rendered images to the window.
		if(glfwCreateWindowSurface(m_vkInstance, m_window, nullptr, &m_surface) != VK_SUCCESS)
		{
			return; // TODO: Log this once error handling system exists.
		}

		std::uint32_t deviceCount = 0;
		m_vulkan->vkEnumeratePhysicalDevices(m_vkInstance, &deviceCount, nullptr);
		// Make sure there actually is a Vulkan-capable GPU available on the system.
		if(deviceCount == 0)
		{
			return; // TODO: Log this once error handling system exists.
		}

		#pragma warning (push)
		#pragma warning (disable : 6385) // Disable warning about reading out of bounds. The size won't grow between these two calls.
		// List the available physical devices (GPUs) that support Vulkan.
		VkPhysicalDevice *devices = new VkPhysicalDevice[deviceCount];
		m_vulkan->vkEnumeratePhysicalDevices(m_vkInstance, &deviceCount, devices);
		for(std::uint32_t deviceIdx = 0; deviceIdx < deviceCount; ++deviceIdx)
		{
			std::uint32_t queueFamilyCount = 0;
			m_vulkan->vkGetPhysicalDeviceQueueFamilyProperties(devices[deviceIdx], &queueFamilyCount, nullptr);

			VkQueueFamilyProperties *queueFamilies = new VkQueueFamilyProperties[queueFamilyCount];
			m_vulkan->vkGetPhysicalDeviceQueueFamilyProperties(devices[deviceIdx], &queueFamilyCount, queueFamilies);

			// Find a queue family that can draw and show images on the surface.
			for(std::uint32_t queueIdx = 0; queueIdx < queueFamilyCount; ++queueIdx)
			{
				VkBool32 canPresent = VK_FALSE;
				m_vulkan->vkGetPhysicalDeviceSurfaceSupportKHR(devices[deviceIdx], queueIdx, m_surface, &canPresent);

				// Check that the family supports graphics operations and can also present to a surface.
				if((queueFamilies[queueIdx].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0 && canPresent == VK_TRUE)
				{
					m_activeDevice = devices[deviceIdx];
					m_activeQueueFamilyIndex = queueIdx;
					break;
				}
			}
			delete[] queueFamilies;
			if(m_activeDevice != nullptr)
				break;
		}
		#pragma warning (pop)
		delete[] devices;

		if(m_activeDevice == nullptr)
		{
			return; // TODO: Log this once error handling system exists.
		}
	}

	void WindowSystem::update(std::chrono::steady_clock::duration dt)
	{
		if(m_window == nullptr)
			return;

		glfwPollEvents();

		if(glfwWindowShouldClose(m_window) || glfwGetKey(m_window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
			m_windowSystemBroadcaster.broadcast(*m_shutdownChannel, true);
	}

	void WindowSystem::render()
	{
		// Render the window contents. Nothing to render yet.
	}

	void WindowSystem::onTearDown()
	{
		// Clean up Vulkan resources and destroy the Vulkan instance.
		if(m_vkInstance != nullptr)
		{
			if(m_surface != nullptr)
			{
				m_vulkan->vkDestroySurfaceKHR(m_vkInstance, m_surface, nullptr);
				m_surface = nullptr;
			}

			m_vulkan->vkDestroyInstance(m_vkInstance, nullptr);
			m_vkInstance = nullptr;
		}
		delete m_vulkan;
		m_vulkan = nullptr;

		if(m_window != nullptr)
		{
			glfwDestroyWindow(m_window);
			m_window = nullptr;
		}
		glfwTerminate();
	}
}