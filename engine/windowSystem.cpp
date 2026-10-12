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
		//////////////////
		// Initialize GLFW
		//////////////////

		// Initialize the GLFW context and validate that it was successful.
		if(glfwInit() == GLFW_FALSE)
			return; // TODO: Log this once error handling system exists.

		// Set configuration hints for the GLFW window. We don't need an OpenGL context since we'll be using Vulkan.
		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); // No OpenGL context; we'll use Vulkan later.
		glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE); // TODO: This is temporary, but the resizing mechanism comes later.

		// Create the window with the specified width, height, and title. The last two parameters are for monitor and shared context, which we don't need.
		// Monitor is for fullscreen mode, and share is a shared context is for sharing OpenGL contexts.
		m_window = glfwCreateWindow(m_windowWidth, m_windowHeight, m_windowTitle.c_str(), nullptr, nullptr);

		////////////////////////////////////////////////////////////////
		// Initialize core Vulkan functions and create a Vulkan instance
		////////////////////////////////////////////////////////////////

		// Create a new VulkanFunctions object to hold the function pointers for Vulkan functions.
		m_vulkan = new vulkan::VulkanFunctions();
		// Get the initial function pointer for vkGetInstanceProcAddr from GLFW. This function is used to load other Vulkan functions.
		PFN_vkGetInstanceProcAddr vulcanLocator = reinterpret_cast<PFN_vkGetInstanceProcAddr>(glfwGetInstanceProcAddress(nullptr, "vkGetInstanceProcAddr"));
		// Pass it to the Vulkan bootstrapper to load the global functions. If it fails, we log and return.
		if(vulcanLocator == nullptr || !vulkan::loadGlobalfunctions(*m_vulkan, vulcanLocator))
			// Nothing to do here; the engine will fail to start if Vulkan is not available. Just log this and return.
			// TODO: Log this once error handling system exists.
			return;

		// Get the required Vulkan instance extensions from GLFW. These are necessary for creating a Vulkan instance that can interface with the window system.
		std::uint32_t instanceExtensionCount = 0;
		// The extensions should be VK_KHR_surface and a platform-specific extension like VK_KHR_win32_surface, required for drawing to a window.
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
		appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0); // Verion number for this application. Can be anything, should match the version of the game.
		appInfo.apiVersion = VK_API_VERSION_1_4;

		// Create a Vulkan "InstanceCreateInfo" structure. This is used to specify the parameters for creating a Vulkan instance.
		VkInstanceCreateInfo instanceInfo = VkInstanceCreateInfo();
		instanceInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		instanceInfo.pApplicationInfo = &appInfo;
		instanceInfo.enabledExtensionCount = instanceExtensionCount;
		instanceInfo.ppEnabledExtensionNames = extensions;

		/////////////////////////////////////////
		// Vulkan Debugging and Validation Layers
		/////////////////////////////////////////
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
			instanceInfo.enabledLayerCount = 1;
			instanceInfo.ppEnabledLayerNames = validationLayers;
		}
		// TODO: Else, record debug-level log that the system does not have the validation layer available.
		// TODO: Add a debug messenger (VK_EXT_debug_utils) once a renderer system exists, to route validation messages to the engine's log.
		// Until then, the validation layer prints its own messages to the console and, on Windows, the debugger output.
#endif
		/////////////////////////////////////////////////////////////////////////////
		// Create vulkan instance, load instance-level functions, create GLFW surface
		/////////////////////////////////////////////////////////////////////////////

		// Create the Vulkan instance using the configuration specified in the createInfo structure.
		if(m_vulkan->vkCreateInstance(&instanceInfo, nullptr, &m_vkInstance) != VK_SUCCESS)
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

		///////////////////////////////////////////////////
		// Select a Vulkan physical device and queue family
		///////////////////////////////////////////////////

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
					m_physicalDevice = devices[deviceIdx];
					m_activeQueueFamilyIndex = queueIdx;
					break;
				}
			}
			delete[] queueFamilies;
			if(m_physicalDevice != nullptr)
				break;
		}
		#pragma warning (pop)
		delete[] devices;

		if(m_physicalDevice == nullptr)
		{
			return; // TODO: Log this once error handling system exists.
		}
		
		/////////////////////////////////////////
		// Create Vulkan logical device and queue
		/////////////////////////////////////////

		// Create a queue from the family and physical device selected.
		// Priority is a float between 0.0 and 1.0, where 1.0 is the highest priority. This is used to determine how the GPU schedules work across multiple queues.
		// For now, with one queue, that doesn't matter.
		float queuePriority = 1.0f;
		VkDeviceQueueCreateInfo queueInfo = VkDeviceQueueCreateInfo();
		queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueInfo.queueFamilyIndex = m_activeQueueFamilyIndex;
		queueInfo.queueCount = 1;
		queueInfo.pQueuePriorities = &queuePriority;

		// Enable dynamic rendering and synchronization features for the logical device.
		// TODO: Check whether these features are actually supported by the physical device before enabling them. If not, we should log and return. This is useful if there are multiple GPUs with different capabilities on the system and a fallback can be selected.
		// Required features can be checked with vkGetPhysicalDeviceFeatures2 and the appropriate feature structure. Feature structures can be chained together with the pNext pointer.
		//   This should be handled by device creation returning VK_ERROR_FEATURE_NOT_PRESENT
		VkPhysicalDeviceVulkan13Features deviceFeatures = VkPhysicalDeviceVulkan13Features();
		deviceFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
		deviceFeatures.dynamicRendering = VK_TRUE;
		deviceFeatures.synchronization2 = VK_TRUE;

		const char *deviceExtensions[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

		VkDeviceCreateInfo deviceInfo = VkDeviceCreateInfo();
		deviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		deviceInfo.pNext = &deviceFeatures; // pNext is used to chain additional structures to the create info. It functions as a linked list. TODO: Figure out how else this is used
		deviceInfo.queueCreateInfoCount = 1;
		deviceInfo.pQueueCreateInfos = &queueInfo;
		deviceInfo.enabledExtensionCount = 1;
		deviceInfo.ppEnabledExtensionNames = deviceExtensions;

		// Create the logical device along with its associated queue
		if(m_vulkan->vkCreateDevice(m_physicalDevice, &deviceInfo, nullptr, &m_device) != VK_SUCCESS)
		{
			return; // TODO: Log this once error handling system exists.
		}

		m_deviceFunctions = new vulkan::DeviceFunctions();
		vulkan::loadDeviceFunctions(*m_deviceFunctions, *m_vulkan, m_device);

		// Fetch the queue from the logical device
		m_deviceFunctions->vkGetDeviceQueue(m_device, m_activeQueueFamilyIndex, 0, &m_queue);

		////////////////////////////
		// Create a Vulkan swapchain
		////////////////////////////

		// Check surface capabilities to determine the supported formats, present modes, etc.
		VkSurfaceCapabilitiesKHR surfaceCapabilities = VkSurfaceCapabilitiesKHR();
		m_vulkan->vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_physicalDevice, m_surface, &surfaceCapabilities);

		// Choose a pixel format.
		std::uint32_t formatCount = 0;
		m_vulkan->vkGetPhysicalDeviceSurfaceFormatsKHR(m_physicalDevice, m_surface, &formatCount, nullptr);
		VkSurfaceFormatKHR *surfaceFormats = new VkSurfaceFormatKHR[formatCount];
		m_vulkan->vkGetPhysicalDeviceSurfaceFormatsKHR(m_physicalDevice, m_surface, &formatCount, surfaceFormats);

		// Default to the first format in the list, but prefer VK_FORMAT_B8G8R8A8_SRGB with VK_COLOR_SPACE_SRGB_NONLINEAR_KHR if available.
		// This format is standard for sRGB color space on desktops and is widely supported.
		VkSurfaceFormatKHR surfaceFormat = surfaceFormats[0];
		for(std::uint32_t formatIdx = 0; formatIdx < formatCount; ++formatIdx)
		{
			if(surfaceFormats[formatIdx].format == VK_FORMAT_B8G8R8A8_SRGB && surfaceFormats[formatIdx].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
			{
				surfaceFormat = surfaceFormats[formatIdx];
				break;
			}
		}
		delete[] surfaceFormats;

		VkExtent2D swapchainExtent = surfaceCapabilities.currentExtent;
		if(swapchainExtent.width == UINT32_MAX || swapchainExtent.height == UINT32_MAX)
		{
			// If the surface size is undefined, we can set it to our desired size.
			int width = 0;
			int height = 0;
			glfwGetFramebufferSize(m_window, &width, &height);
			swapchainExtent.width = (std::uint32_t)width;
			swapchainExtent.height = (std::uint32_t)height;
		}

		// Ensure there is at least one more image than the minimum required by the surface. This allows for buffering.
		std::uint32_t imageCount = surfaceCapabilities.minImageCount + 1;
		if(surfaceCapabilities.maxImageCount > 0 && imageCount > surfaceCapabilities.maxImageCount)
		{
			imageCount = surfaceCapabilities.maxImageCount;
		}
		VkSwapchainCreateInfoKHR swapchainInfo = VkSwapchainCreateInfoKHR();
		swapchainInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
		swapchainInfo.surface = m_surface;
		swapchainInfo.minImageCount = imageCount;
		swapchainInfo.imageFormat = surfaceFormat.format;
		swapchainInfo.imageColorSpace = surfaceFormat.colorSpace;
		swapchainInfo.imageExtent = swapchainExtent;
		swapchainInfo.imageArrayLayers = 1; // 1 for normal 2D images, 2 for stereoscopic 3D images.
		swapchainInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT; // Tells Vulkan that we will draw to these images.
		swapchainInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE; // Images are owned by one queue family at a time.
		swapchainInfo.preTransform = surfaceCapabilities.currentTransform; // No transformation is applied to the images. This keeps whatever the surface is currently doing.
		swapchainInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR; // Don't use the alpha channel. Makes the entire window opaque. This is standard for most applications.
		swapchainInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR; // Display frames in order, one per vertical refresh. This is the only mode guaranteed to be available on all platforms.
		swapchainInfo.clipped = VK_TRUE; // Don't care about pixels that are obscured by other windows. This allows the GPU to optimize rendering. On Windows, this won't matter for screen recording, but apparently that's because of how Windows composites windows. And also apparently this can't be flipped during runtime, so the context would need to be recreated if the user wants to change it.

		if(m_deviceFunctions->vkCreateSwapchainKHR(m_device, &swapchainInfo, nullptr, &m_swapchain) != VK_SUCCESS)
		{
			return; // TODO: Log this once error handling system exists.
		}

		// Store the swapchain format and dimensions for later use.
		m_swapchainFormat = (uint32_t)surfaceFormat.format;
		m_swapchainWidth = swapchainExtent.width;
		m_swapchainHeight = swapchainExtent.height;

		///////////////////////////////////////////////////////////
		// Create and retrieve the swapchain image views and images
		///////////////////////////////////////////////////////////

		m_deviceFunctions->vkGetSwapchainImagesKHR(m_device, m_swapchain, &m_swapchainImageCount, nullptr);
		m_swapchainImages = new VkImage[m_swapchainImageCount];
		m_deviceFunctions->vkGetSwapchainImagesKHR(m_device, m_swapchain, &m_swapchainImageCount, m_swapchainImages);

		// Create an image view for each swapchain image. Image views describe how the images will be accessed and used in shaders.
		// The array is value-initialized to nullptr so that if part of the array fails to initialize it can still be destroyed smoothly.
		m_swapchainImageViews = new VkImageView[m_swapchainImageCount]();

		for(std::uint32_t imageIdx = 0; imageIdx < m_swapchainImageCount; ++imageIdx)
		{
			VkImageViewCreateInfo viewInfo = VkImageViewCreateInfo();
			viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			viewInfo.image = m_swapchainImages[imageIdx];
			viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
			viewInfo.format = surfaceFormat.format;
			// subresourceRange declares what part of an image the view is for. It can be configure for mipmaps, array layers, and aspects (color, depth, stencil).
			viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			viewInfo.subresourceRange.baseMipLevel = 0;
			viewInfo.subresourceRange.levelCount = 1;
			viewInfo.subresourceRange.baseArrayLayer = 0;
			viewInfo.subresourceRange.layerCount = 1;
			//view.components.r also exists, along with g, b, and a, for remapping with VK_COMPONENT_SWIZZLE_* values.
			
			if(m_deviceFunctions->vkCreateImageView(m_device, &viewInfo, nullptr, &m_swapchainImageViews[imageIdx]) != VK_SUCCESS)
			{
				return; // TODO: Log this once error handling system exists.
			}
		}

		////////////////////////////////////////
		// Create Vulkan command pool and buffer
		////////////////////////////////////////

		VkCommandPoolCreateInfo poolInfo = VkCommandPoolCreateInfo();
		poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		// Allow each buffer to be individually reset. This is useful for re-recording command buffers without having to reset the entire pool.
		poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
		poolInfo.queueFamilyIndex = m_activeQueueFamilyIndex;

		// Create the command pool
		if(m_deviceFunctions->vkCreateCommandPool(m_device, &poolInfo, nullptr, &m_commandPool) != VK_SUCCESS)
		{
			return; // TODO: Log this once error handling system exists.
		}

		VkCommandBufferAllocateInfo bufferInfo = VkCommandBufferAllocateInfo();
		bufferInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		bufferInfo.commandPool = m_commandPool;
		bufferInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY; // Primary buffers can be submitted to a queue, while secondary buffers can only be called from primary buffers.
		bufferInfo.commandBufferCount = 1;

		// Allocate the command buffer
		if(m_deviceFunctions->vkAllocateCommandBuffers(m_device, &bufferInfo, &m_commandBuffer) != VK_SUCCESS)
		{
			return; // TODO: Log this once error handling system exists.
		}

		/////////////////////////////////////
		// Create Vulkan semaphores and fence
		/////////////////////////////////////

		VkSemaphoreCreateInfo semaphoreInfo = VkSemaphoreCreateInfo();
		semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
		if(m_deviceFunctions->vkCreateSemaphore(m_device, &semaphoreInfo, nullptr, &m_imageAvailable) != VK_SUCCESS)
		{
			return; // TODO: Log this once error handling system exists.
		}

		// Create one semaphore for each swapchain image to signal when rendering is finished.
		m_renderFinished = new VkSemaphore[m_swapchainImageCount]();
		for(std::uint32_t imageIdx = 0; imageIdx < m_swapchainImageCount; ++imageIdx)
		{
			if(m_deviceFunctions->vkCreateSemaphore(m_device, &semaphoreInfo, nullptr, &m_renderFinished[imageIdx]) != VK_SUCCESS)
			{
				return; // TODO: Log this once error handling system exists.
			}
		}

		// Create a fence to synchronize CPU and GPU work.
		VkFenceCreateInfo fenceInfo = VkFenceCreateInfo();
		fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		// Create the fence in a signaled state so that the first frame can be rendered without waiting.
		fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
		if(m_deviceFunctions->vkCreateFence(m_device, &fenceInfo, nullptr, &m_frameFence) != VK_SUCCESS)
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
		// Make sure setup was successful before trying to render. If not, just return.
		// The fence was the last thing created in setup, so if it is nullptr, we know setup did not complete successfully.
		// TODO: We shouldn't need this. The engine should not call render() if setup failed. This is a temporary measure until the engine has a proper error handling system.
		if(m_frameFence == nullptr)
			return;

		// Wait until the GPU hhas finished the previous frame.
		m_deviceFunctions->vkWaitForFences(m_device, 1, &m_frameFence, VK_TRUE, UINT64_MAX);

		// Get the next image in the swapchain.
		std::uint32_t imageIndex = 0;
		// Wait on the semaphore that signals when the image is available.
		VkResult acquireResult = m_deviceFunctions->vkAcquireNextImageKHR(m_device, m_swapchain, UINT64_MAX, m_imageAvailable, nullptr, &imageIndex);
		if(acquireResult != VK_SUCCESS && acquireResult != VK_SUBOPTIMAL_KHR)
		{
			// VK_SUBOPTIMAL_KHR is allowed, but it means the swapchain is no longer optimal for the surface. This can happen if the window is resized, for example.
			// There are a dozen codes that can be returned. See https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/vkAcquireNextImageKHR.html for the full list.
			return; // Skip this frame. TODO: Recreate the swapchain once resizing is implemented.
		}

		// This has to be done here, after the frame may be skipped, because if it were set first and the frame were skipped,
		// nothing would reset the fence and the next frame would wait forever.
		m_deviceFunctions->vkResetFences(m_device, 1, &m_frameFence);

		// Start recording commands
		VkCommandBufferBeginInfo beginInfo = VkCommandBufferBeginInfo();
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		// I don't understand why this is recreated every frame.
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		m_deviceFunctions->vkBeginCommandBuffer(m_commandBuffer, &beginInfo);
		// Beginning clears out the command buffer because RESET_COMMAND_BUFFER_BIT was set when the command pool was created.
		// If it weren't set, then individual command buffers couldn't be reset and would have to be re-recorded every frame.

		// Prepare the image for rendering.
		VkImageMemoryBarrier2 toDrawing = VkImageMemoryBarrier2();
		toDrawing.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
		// GPU operations run in stages.
		// See https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/VkPipelineStageFlagBits2.html for a list of stage bits.
		// The color attachment output stage is where the final color values are written to the image.
		toDrawing.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
		toDrawing.srcAccessMask = 0;
		toDrawing.dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
		toDrawing.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
		toDrawing.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED; // This value means we don't care about the previous data.
		toDrawing.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		toDrawing.image = m_swapchainImages[imageIndex];
		toDrawing.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		toDrawing.subresourceRange.levelCount = 1;
		toDrawing.subresourceRange.layerCount = 1;

		// No idea what this is. This is way too confusing
		VkDependencyInfo toDrawingDependency = VkDependencyInfo();
		toDrawingDependency.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
		toDrawingDependency.imageMemoryBarrierCount = 1;
		toDrawingDependency.pImageMemoryBarriers = &toDrawing;
		// Pass the magical dependency to the command buffer
		m_deviceFunctions->vkCmdPipelineBarrier2(m_commandBuffer, &toDrawingDependency);

		// Clear the image to a solid color.
		VkRenderingAttachmentInfo colorAttachment =  VkRenderingAttachmentInfo();
		colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
		// Bind the current swapchain image to the color attachment.
		colorAttachment.imageView = m_swapchainImageViews[imageIndex];
		colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		// CLEAR fills the image with  clearValue when drawing begins, and STORE keeps the result afterward.
		colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		// RGBA
		colorAttachment.clearValue.color.float32[0] = 0.1f;
		colorAttachment.clearValue.color.float32[1] = 0.1f;
		colorAttachment.clearValue.color.float32[2] = 0.2f;
		colorAttachment.clearValue.color.float32[3] = 1.0f;

		VkRenderingInfo renderingInfo = VkRenderingInfo();
		renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
		renderingInfo.renderArea.extent.width = m_swapchainWidth;
		renderingInfo.renderArea.extent.height = m_swapchainHeight;
		renderingInfo.layerCount = 1;
		renderingInfo.colorAttachmentCount = 1;
		renderingInfo.pColorAttachments = &colorAttachment;

		m_deviceFunctions->vkCmdBeginRendering(m_commandBuffer, &renderingInfo);
		// Later, draw calls will go here.
		m_deviceFunctions->vkCmdEndRendering(m_commandBuffer);

		// Prepare the image for presentation.
		VkImageMemoryBarrier2 toPresent = VkImageMemoryBarrier2();
		toPresent.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
		// What are these stages? This is the same as the settings for toDrawing, but the srcAccessMask is set this time.
		toPresent.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
		toPresent.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
		// ???
		toPresent.dstStageMask = VK_PIPELINE_STAGE_2_NONE;
		toPresent.dstAccessMask = 0;
		toPresent.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		toPresent.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		toPresent.image = m_swapchainImages[imageIndex];
		// Where do the subresource values come from?
		toPresent.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		toPresent.subresourceRange.levelCount = 1;
		toPresent.subresourceRange.layerCount = 1;

		VkDependencyInfo toPresentDependency = VkDependencyInfo();
		toPresentDependency.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
		toPresentDependency.imageMemoryBarrierCount = 1;
		toPresentDependency.pImageMemoryBarriers = &toPresent;
		m_deviceFunctions->vkCmdPipelineBarrier2(m_commandBuffer, &toPresentDependency);

		m_deviceFunctions->vkEndCommandBuffer(m_commandBuffer);

		// Info structs for subbmitting the semaphores and command buffer to the queue.
		VkSemaphoreSubmitInfo waitInfo = VkSemaphoreSubmitInfo();
		waitInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
		waitInfo.semaphore = m_imageAvailable;
		waitInfo.stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;

		VkSemaphoreSubmitInfo signalInfo = VkSemaphoreSubmitInfo();
		signalInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
		signalInfo.semaphore = m_renderFinished[imageIndex];
		signalInfo.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;

		VkCommandBufferSubmitInfo commandInfo = VkCommandBufferSubmitInfo();
		commandInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
		commandInfo.commandBuffer = m_commandBuffer;

		// Bind the info structs to a submit info struct
		VkSubmitInfo2 submitInfo = VkSubmitInfo2();
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
		submitInfo.waitSemaphoreInfoCount = 1;
		submitInfo.pWaitSemaphoreInfos = &waitInfo;
		submitInfo.commandBufferInfoCount = 1;
		submitInfo.pCommandBufferInfos = &commandInfo;
		submitInfo.signalSemaphoreInfoCount = 1;
		submitInfo.pSignalSemaphoreInfos = &signalInfo;

		// Submit the work to the queue and signal the fence when it is done.
		m_deviceFunctions->vkQueueSubmit2(m_queue, 1, &submitInfo, m_frameFence);

		// Present the image to the surface.
		VkPresentInfoKHR presentInfo = VkPresentInfoKHR();
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
		// Wait on the semaphore that signals when rendering is finished.
		presentInfo.waitSemaphoreCount = 1;
		presentInfo.pWaitSemaphores = &m_renderFinished[imageIndex];
		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = &m_swapchain;
		presentInfo.pImageIndices = &imageIndex;

		// Present the image to the surface.
		m_deviceFunctions->vkQueuePresentKHR(m_queue, &presentInfo);
	}

	void WindowSystem::onTearDown()
	{
		// Synchronize before destroying resources to ensure that all GPU operations are complete.
		if(m_device != nullptr)
		{
			m_deviceFunctions->vkDeviceWaitIdle(m_device);
		}

		if(m_frameFence != nullptr)
		{
			m_deviceFunctions->vkDestroyFence(m_device, m_frameFence, nullptr);
			m_frameFence = nullptr;
		}

		if(m_renderFinished != nullptr)
		{
			for(std::uint32_t imageIdx = 0; imageIdx < m_swapchainImageCount; ++imageIdx)
			{
				if(m_renderFinished[imageIdx] != nullptr)
					m_deviceFunctions->vkDestroySemaphore(m_device, m_renderFinished[imageIdx], nullptr);
			}
			delete[] m_renderFinished;
			m_renderFinished = nullptr;
		}

		if(m_imageAvailable != nullptr)
		{
			m_deviceFunctions->vkDestroySemaphore(m_device, m_imageAvailable, nullptr);
			m_imageAvailable = nullptr;
		}

		if(m_commandPool != nullptr)
		{
			m_deviceFunctions->vkDestroyCommandPool(m_device, m_commandPool, nullptr);
			m_commandPool = nullptr;
			m_commandBuffer = nullptr; // Command buffers are freed when the pool is destroyed, so we don't need to free it explicitly.
		}

		// Clean up Vulkan resources and destroy the Vulkan instance.
		if(m_swapchainImageViews != nullptr)
		{
			for(std::uint32_t imageIdx = 0; imageIdx < m_swapchainImageCount; ++imageIdx)
			{
				if(m_swapchainImageViews[imageIdx] != nullptr)
					m_deviceFunctions->vkDestroyImageView(m_device, m_swapchainImageViews[imageIdx], nullptr);
			}
			delete[] m_swapchainImageViews;
			m_swapchainImageViews = nullptr;
		}

		if(m_swapchainImages != nullptr)
		{
			delete[] m_swapchainImages;
			m_swapchainImages = nullptr;
		}

		if(m_swapchain != nullptr)
		{
			m_deviceFunctions->vkDestroySwapchainKHR(m_device, m_swapchain, nullptr);
			m_swapchain = nullptr;
		}

		if(m_device != nullptr)
		{
			m_deviceFunctions->vkDestroyDevice(m_device, nullptr);
			m_device = nullptr;
		}

		if(m_surface != nullptr)
		{
			m_vulkan->vkDestroySurfaceKHR(m_vkInstance, m_surface, nullptr);
			m_surface = nullptr;
		}
		if(m_vkInstance != nullptr)
		{
			m_vulkan->vkDestroyInstance(m_vkInstance, nullptr);
			m_vkInstance = nullptr;
		}

		delete m_deviceFunctions;
		m_deviceFunctions = nullptr;

		delete m_vulkan;
		m_vulkan = nullptr;

		// Clean up GLFW resources and terminate the GLFW context.
		if(m_window != nullptr)
		{
			glfwDestroyWindow(m_window);
			m_window = nullptr;
		}
		glfwTerminate();
	}
}