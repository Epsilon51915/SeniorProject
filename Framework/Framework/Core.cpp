#include "Core.h"



// Init Context and Instance
void initVulkan()
{
	constexpr vk::ApplicationInfo appInfo
	{
		.pApplicationName = "Framework",
		.applicationVersion = VK_MAKE_VERSION(1, 0, 0),
		.pEngineName = "No engine",
		.engineVersion = VK_MAKE_VERSION(1, 0 ,0),
		.apiVersion = vk::ApiVersion14
	};

	// Get Vulkan instance extensions from GLFW
	uint32_t glfwExtensionCount = 0;
	auto glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

	// Loop through and find extensions
	auto extensionProperties = vkContext.enumerateInstanceExtensionProperties();
	for (int x = 0; x < glfwExtensionCount; ++x)
	{
		if (std::ranges::none_of(extensionProperties,
			[glfwExtension = glfwExtensions[x]](auto const& extensionProperty)
			{ return strcmp(extensionProperty.extensionName, glfwExtension) == 0; }))
		{
			throw std::runtime_error("Required GLFW extension not supported: " + std::string(glfwExtensions[x]));
		}
	}


	vk::InstanceCreateInfo createInfo
	{
		.pApplicationInfo = &appInfo,
		.enabledExtensionCount = glfwExtensionCount,
		.ppEnabledExtensionNames = glfwExtensions
	};
	vkInstance = vk::raii::Instance(vkContext, createInfo);

}
