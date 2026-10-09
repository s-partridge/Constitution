#ifndef CGE_VULKAN_BOOTSTRAPPER_H
#define CGE_VULKAN_BOOTSTRAPPER_H

// Include this header before anything else that includes vulkan.h.
#ifndef VK_NO_PROTOTYPES
#define VK_NO_PROTOTYPES
#endif
#include <vulkan/vulkan.h>

#endif // CGE_VULKAN_BOOTSTRAPPER_H
