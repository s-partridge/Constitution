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
		// TODO: Log this once error handling system exists.
		if(glfwInit() == GLFW_FALSE)
			return;

		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); // No OpenGL context; we'll use Vulkan later.
		m_window = glfwCreateWindow(m_windowWidth, m_windowHeight, m_windowTitle.c_str(), nullptr, nullptr);

		m_vulkan = new vulkan::InstanceFunctions();
		PFN_vkGetInstanceProcAddr vulcanLocator = reinterpret_cast<PFN_vkGetInstanceProcAddr>(glfwGetInstanceProcAddress(nullptr, "vkGetInstanceProcAddr"));

		if(vulcanLocator == nullptr || !vulkan::loadGlobalfunctions(*m_vulkan, vulcanLocator))
			// Nothing to do here; the engine will fail to start if Vulkan is not available. Just log this and return.
			// TODO: Log this once error handling system exists.
			return;

		std::uint32_t instanceExtensionCount = 0;
		const char **extensions = glfwGetRequiredInstanceExtensions(&instanceExtensionCount);

		VkApplicationInfo appInfo = VkApplicationInfo();
		appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		appInfo.pApplicationName = m_windowTitle.c_str();
		appInfo.pEngineName = "Constitution";
		appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0); // No idea whether this is useful. TODO: Make this configurable.
		appInfo.apiVersion = VK_API_VERSION_1_4;

		VkInstanceCreateInfo createInfo = VkInstanceCreateInfo();
		createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		createInfo.pApplicationInfo = &appInfo;
		createInfo.enabledExtensionCount = instanceExtensionCount;
		createInfo.ppEnabledExtensionNames = extensions;

#ifdef _DEBUG
		const char *validationLayers[] = { "VK_LAYER_KHRONOS_validation" };
		createInfo.enabledLayerCount = 1;
		createInfo.ppEnabledLayerNames = validationLayers;
#endif
		if(m_vulkan->vkCreateInstance(&createInfo, nullptr, &m_vkInstance) != VK_SUCCESS)
		{
			// TODO: Log this once error handling system exists.
			return;
		}

		loadInstanceFunctions(*m_vulkan, m_vkInstance);
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
		if(m_vkInstance != nullptr)
		{
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