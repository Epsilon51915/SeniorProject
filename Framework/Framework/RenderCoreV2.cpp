#include "RenderCoreV2.hpp"


namespace gpu {

	// Important program variables
	constexpr uint32_t maxDeviceExtensions	 = 512;
	constexpr uint32_t maxInstanceExtensions = 512;
	constexpr uint32_t maxInstanceLayers	 = 64;
	constexpr uint32_t maxSurfaceFormats	 = 64;
	constexpr uint32_t queueTypeCount		 = 3; // The 3 queues: Graphics, Compute, Transfer.
	constexpr uint32_t maxSwapchainImages	 = 3;


	// Definition of forward declared structs
	struct Device
	{
		// Core program variables
		VkDevice device		= VK_NULL_HANDLE;
		VkInstance instance = VK_NULL_HANDLE;
		Swapchain* swapchain			= nullptr;
		Swapchain* acquiredSwapchain	= nullptr;
		Queue*	queueArray				= nullptr;

		// Device Information/Properties
		// TODO: research on whether to use VkPhysicalDeviceMemoryProperties or VkPhysicalDeviceMemoryProperties2
		VkPhysicalDeviceProperties deviceProperties;
		VkPhysicalDeviceMemoryProperties deviceMemProps{};
		VkPhysicalDeviceDescriptorHeapPropertiesEXT deviceHeapProps{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_HEAP_PROPERTIES_EXT };

		// Queue Information
		uint32_t totalQueueCount	= 0;
		uint32_t queueFamilyCount	= 0;
		uint32_t graphicsQueueCount = 0;
		uint32_t computeQueueCount	= 0;
		uint32_t transferQueueCount = 0;
		uint32_t queueFamiliesIndex[queueTypeCount]{};

		// Texture Information
		uint64_t textureHeapAlign = 16; // 16 what? Define this.
		uint32_t textureMemoryType = VK_MAX_MEMORY_TYPES;
		bool textureCompressionEtc2		= false;
		bool swapchainColorSpaceEnabled = false;

		// Swapchain/Present information
		PresentContext presentContexts[maxSwapchainImages]{};
		uint32_t presentContextCount = 0;
		uint32_t nextPresentContext = 0;

	};

	// The queue counts, features, and other aspects of the physical/logical device we are using
	struct DeviceSpecs
	{

	};

	DeviceInit createDevice()
	{
		Device* device = new Device;

		// Create VK instance

		return DeviceInit();
	}
}