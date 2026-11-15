#include "RenderCoreV2.hpp"


namespace core {

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
	constexpr const char* requiredInstanceExtensions[] =
	{
		VK_KHR_SURFACE_EXTENSION_NAME, // ALWAYS required by GLFW
		VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME,
		VK_EXT_SWAPCHAIN_COLOR_SPACE_EXTENSION_NAME
	};


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
		VkDevice device					= VK_NULL_HANDLE;
		VkInstance instance				= VK_NULL_HANDLE;
		VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
		VkSurfaceKHR surface			= VK_NULL_HANDLE;
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

		~Device();

		Device() = default;
		Device(const Device&) = delete;
		Device& operator=(const Device&) = delete;

	};

	// Filled in when inspecting potential devices
	struct Candidate
	{
		VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
		uint32_t queueFamilies[queueTypeCount]{};
		uint32_t queueCounts[queueTypeCount]{};
		bool timestamps[queueTypeCount]{};
	};

	// Checks if device we are inspecting has required features
	void inspectDevice(const VkPhysicalDevice device, const char* maintenanceExtensionName, Candidate& output)
	{
		std::vector<VkExtensionProperties> supportedDeviceExtensions;
		if (getDeviceInstanceExtensions(supportedDeviceExtensions, device) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to get device instance extensnions");
		}
	}

	bool hasName(const char* extensionName, const char* nameCheck)
	{
		if (strcmp(nameCheck, extensionName) == 0)
		{
			return true;
		}
		return false;
	}

	VkResult getInstanceExtensions(std::vector<VkExtensionProperties>& extensions)
	{
		uint32_t count = 0;
		VkResult result = VK_SUCCESS;

		// Get instance count
		vkEnumerateInstanceExtensionProperties(nullptr, &count, nullptr);
		extensions.resize(count);
		// Get those extensions
		result = vkEnumerateInstanceExtensionProperties(nullptr, &count, extensions.data());
		return result;
	}
	VkResult getDeviceInstanceExtensions(std::vector<VkExtensionProperties>& extensions, const VkPhysicalDevice device)
	{
		uint32_t count = 0;
		VkResult result = VK_SUCCESS;

		// Get instance count
		vkEnumerateDeviceExtensionProperties(device, nullptr, &count, nullptr);
		extensions.resize(count);
		// Get those extensions
		result = vkEnumerateDeviceExtensionProperties(device, nullptr, &count, extensions.data());
		return result;
	}
	VkResult getInstanceLayerProperties(std::vector<VkLayerProperties>& extensions) 
	{
		uint32_t count = 0;
		VkResult result = VK_SUCCESS;

		// Get instance count
		vkEnumerateInstanceLayerProperties(&count, nullptr);
		extensions.resize(count);
		// Get those extensions
		result = vkEnumerateInstanceLayerProperties(&count, extensions.data());
		return result;
	}

	DeviceInit createDevice()
	{
		// TODO: This is broken somehow
		Device* device = new Device;
		std::vector<VkExtensionProperties> supportedInstanceExtensions;
		std::vector<VkLayerProperties> supportedLayerProperties;
		std::vector<const char*> enabledInstanceExtensions;
		std::vector<const char*> enabledLayers;

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

		// Get extensions and layers
		// noapi queries A LOT of extensions with larger array. Make this more efficient with vectors?
		getInstanceExtensions(supportedInstanceExtensions);
		getInstanceLayerProperties(supportedLayerProperties);

		// After getting extensions, add the properties and names into the enabled arrays
		if (enableValidationLayers)
		{
			enabledLayers.push_back(validationLayers);
			enabledInstanceExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
		}
		// Find which surface maintenance device has, IF it has it
		bool surfaceMaintenanceFound = false;
		const char* foundMaintenanceName = nullptr;
		for (int index = 0; index < supportedInstanceExtensions.size(); ++index)
		{
			if (strcmp(supportedInstanceExtensions.at(index).extensionName, VK_KHR_SURFACE_MAINTENANCE_1_EXTENSION_NAME) == 0)
			{
				enabledInstanceExtensions.push_back(VK_KHR_SURFACE_MAINTENANCE_1_EXTENSION_NAME);
				surfaceMaintenanceFound = true;
				foundMaintenanceName = VK_KHR_SURFACE_MAINTENANCE_1_EXTENSION_NAME;
				break;
			}
			else if (strcmp(supportedInstanceExtensions.at(index).extensionName, VK_EXT_SURFACE_MAINTENANCE_1_EXTENSION_NAME) == 0)
			{
				enabledInstanceExtensions.push_back(VK_EXT_SURFACE_MAINTENANCE_1_EXTENSION_NAME);
				surfaceMaintenanceFound = true;
				foundMaintenanceName = VK_EXT_SURFACE_MAINTENANCE_1_EXTENSION_NAME;
				break;
			}
		}
		if (!surfaceMaintenanceFound)
		{
			throw std::runtime_error("Surface Maintenance Extension not found! May not be supported on device");
		}
		// NOTE: always use pointers or references when using vectors as if you dont it will self-modify its values and mess up your result
		// TODO: add error check for missing extensions
		uint32_t addedExtensions = 0;
		for (const char* name : requiredInstanceExtensions)
		{
			for (const VkExtensionProperties& extension : supportedInstanceExtensions)
			{
				if (strcmp(name, extension.extensionName) == 0)
				{
					enabledInstanceExtensions.push_back(extension.extensionName);
					addedExtensions++;
					break;
				}
			}
		}
		if (addedExtensions < 3)// TODO: remove this hardcoded value later by turning required extensions into a vector
		{
			throw std::runtime_error("Failed to get all required extensions, check debugger for what is mising.");
		}


		const VkInstanceCreateInfo instanceInfo
		{
			.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
			.pApplicationInfo = &appInfo,
			.enabledLayerCount = (uint32_t) enabledLayers.size(),
			.ppEnabledLayerNames = enabledLayers.data(),
			.enabledExtensionCount = (uint32_t) enabledInstanceExtensions.size(),
			.ppEnabledExtensionNames = enabledInstanceExtensions.data()
		};
		if (vkCreateInstance(&instanceInfo, nullptr, &device->instance) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create Vulkan instance!");
		}

		// Get available physical devices
		uint32_t deviceCount = 0;
		std::vector<VkPhysicalDevice> physicalDeviceList;
		VkPhysicalDevice chosenDevice = VK_NULL_HANDLE;
		vkEnumeratePhysicalDevices(device->instance, &deviceCount, nullptr);
		physicalDeviceList.resize(deviceCount);
		vkEnumeratePhysicalDevices(device->instance, &deviceCount, physicalDeviceList.data());
		Candidate selects{};
		for (const VkPhysicalDevice& physicalDevice : physicalDeviceList)
		{
			Candidate candidate{};
			inspectDevice(physicalDevice, foundMaintenanceName, candidate);
		}

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
	//VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData)
	//{



	//	return VK_SUCCESS;
	//}
}