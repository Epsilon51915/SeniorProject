#include "RenderCoreV2.hpp"


namespace gpu {

	// Important program variables
	constexpr uint32_t maxDeviceExtensions	 = 512;
	constexpr uint32_t maxInstanceExtensions = 512;
	constexpr uint32_t maxInstanceLayers	 = 64;
	constexpr uint32_t maxSurfaceFormats	 = 64;
	constexpr uint32_t queueTypeCount		 = 3; // The 3 queues: Graphics, Compute, Transfer.
	constexpr uint32_t maxSwapchainImages	 = 3;


#ifdef NDEBUG
	constexpr bool enableValidationLayers = false;
#else
	constexpr bool enableValidationLayers = true;
#endif


	// Validation layers are an important debugging tool. Pretty much always want this unless compilng for release
	const char* validationLayers = { "VK_LAYER_KHRONOS_validation" };


	struct DeviceFunctions
	{
		PFN_vkWriteSamplerDescriptorsEXT writeSamplerDescriptors = nullptr;
		PFN_vkWriteResourceDescriptorsEXT writeResourceDescriptors = nullptr;
		PFN_vkCmdBindSamplerHeapEXT cmdBindSamplerHeap = nullptr;
		PFN_vkCmdBindResourceHeapEXT cmdBindTextureHeap = nullptr;
		PFN_vkCmdPushDataEXT cmdPushData = nullptr;
		PFN_vkCmdBindIndexBuffer3KHR cmdBindIndexBuffer = nullptr;
		PFN_vkCmdDrawIndirect2KHR cmdDrawIndirect = nullptr;
		PFN_vkCmdDrawIndexedIndirect2KHR cmdDrawIndexedIndirect = nullptr;
		PFN_vkCmdDispatchIndirect2KHR cmdDispatchIndirect = nullptr;
		PFN_vkCmdDrawMeshTasksEXT cmdDrawMeshTasks = nullptr;
		PFN_vkCmdDrawMeshTasksIndirect2EXT cmdDrawMeshTasksIndirect = nullptr;
		PFN_vkCmdDrawMeshTasksIndirectEXT cmdDrawMeshTasksIndirectBuffer = nullptr;
		PFN_vkCmdCopyMemoryKHR cmdCopyMemory = nullptr;
		PFN_vkCmdCopyMemoryToImageKHR cmdCopyMemoryToImage = nullptr;
		PFN_vkCmdCopyImageToMemoryKHR cmdCopyImageToMemory = nullptr;
	};


	// The queue counts, features, and other aspects of the physical/logical device we are using
	// TODO: Consider if this struct is really needed as these variables can honestly be in Device and we have 1 less struct
	struct DeviceSpecs
	{
		const char* deviceName;
		

		// Queue Type Information
		uint32_t totalQueueCount	= 0;
		uint32_t graphicsQueueCount = 0;
		uint32_t computeQueueCount	= 0;
		uint32_t transferQueueCount = 0;
	};

	// Definition of forward declared structs
	struct Device
	{
		// Core program variables
		VkDevice device		= VK_NULL_HANDLE;
		VkInstance instance = VK_NULL_HANDLE;
		Swapchain* swapchain			= nullptr;
		Swapchain* acquiredSwapchain	= nullptr;
		Queue*	queueArray				= nullptr;
		DeviceFunctions functions;
		DeviceSpecs specs;

		// Device Information/Properties
		// TODO: research on whether to use VkPhysicalDeviceMemoryProperties or VkPhysicalDeviceMemoryProperties2
		VkPhysicalDeviceProperties deviceProperties;
		VkPhysicalDeviceMemoryProperties deviceMemProps{};
		VkPhysicalDeviceDescriptorHeapPropertiesEXT deviceHeapProps{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_HEAP_PROPERTIES_EXT };

		// Queue Family Information
		uint32_t queueFamilyCount = 0;
		uint32_t queueFamiliesIndex[queueTypeCount]{};

		// Texture Information
		uint64_t textureHeapAlign		= 16; // 16 what? Define this.
		uint32_t textureMemoryType		= VK_MAX_MEMORY_TYPES;
		bool textureCompressionEtc2		= false;
		bool swapchainColorSpaceEnabled = false;

		// Swapchain/Present Information

		// Present Information
		PresentContext presentContexts[maxSwapchainImages]{};
		VkSemaphore presentationRetirement		 = VK_NULL_HANDLE;
		uint32_t presentContextCount			 = 0;
		uint32_t nextPresentContext				 = 0;
		uint64_t presentationRetirementValue	 = 0;
		uint64_t completedPresentationRetirement = 0;
		uint64_t maxTimelineValueDifference		 = 0;

	};


	DeviceInit createDevice()
	{
		Device* device = new Device;
		uint32_t enabledExtensionCount = 0;
		uint32_t enabledLayerCount	   = 0;
		uint32_t glfwExtensionCount	   = 0;
		VkExtensionProperties enabledInstanceExtensions[maxInstanceExtensions]{};
		const char* enabledLayers[maxInstanceLayers]{};
		// We have to fetch the glfw instances separetely and add it into enabledInstanceExtensions later
		const char** enabledGLFWExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

		// Create VK instance
		const VkApplicationInfo appInfo
		{
			.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
			.pApplicationName = "noapi Framework",
			.applicationVersion = VK_MAKE_API_VERSION(0, 0, 1, 0),
			.pEngineName = "NoGraphicsAPI",
			.engineVersion = VK_MAKE_API_VERSION(0, 0, 1, 0),
			.apiVersion = VK_API_VERSION_1_4,
		};
		VkInstanceCreateInfo instanceInfo;

		// Get device info and create device
		VkDeviceCreateInfo deviceInfo;

		// Create debug util

		return DeviceInit();
	}
	VkResult createDebugUtil()
	{
		VkDebugUtilsMessengerCreateInfoEXT debugUitlInfo;

		return VkResult();
	}
	VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData)
	{



		return VK_SUCCESS;
	}
}