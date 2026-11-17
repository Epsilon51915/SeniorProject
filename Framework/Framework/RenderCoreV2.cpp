#include "RenderCoreV2.hpp"


namespace core {

	// Important program variables
	constexpr uint32_t maxDeviceExtensions	 = 512;
	constexpr uint32_t maxInstanceExtensions = 512;
	constexpr uint32_t maxInstanceLayers	 = 64;
	constexpr uint32_t maxSurfaceFormats	 = 64;
	constexpr uint32_t maxQueueFamilies		 = 64; // So unbeleivably unlikely but sure
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
	constexpr const char* requiredDeviceExtensions[]{
		VK_EXT_DESCRIPTOR_HEAP_EXTENSION_NAME,
		VK_KHR_SHADER_UNTYPED_POINTERS_EXTENSION_NAME,
		VK_EXT_MESH_SHADER_EXTENSION_NAME,
		VK_KHR_SWAPCHAIN_EXTENSION_NAME,
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

	// Vulkan features we want to use
	struct DesiredFeatures
	{
		VkPhysicalDeviceFeatures2 core{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2 };
		VkPhysicalDeviceVulkan11Features vulkan11{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES };
		VkPhysicalDeviceVulkan12Features vulkan12{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES };
		VkPhysicalDeviceVulkan13Features vulkan13{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES };
		VkPhysicalDeviceVulkan14Features vulkan14{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_4_FEATURES };
		VkPhysicalDeviceDescriptorHeapFeaturesEXT descriptorHeap{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_HEAP_FEATURES_EXT };
		VkPhysicalDeviceDeviceAddressCommandsFeaturesKHR addressCommands{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEVICE_ADDRESS_COMMANDS_FEATURES_KHR };
		VkPhysicalDeviceShaderUntypedPointersFeaturesKHR untypedPointers{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_UNTYPED_POINTERS_FEATURES_KHR };
		VkPhysicalDeviceUnifiedImageLayoutsFeaturesKHR unifiedImageLayouts{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_UNIFIED_IMAGE_LAYOUTS_FEATURES_KHR };
		VkPhysicalDeviceMeshShaderFeaturesEXT meshShader{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT };
		VkPhysicalDeviceSwapchainMaintenance1FeaturesKHR swapchainMaintenance1{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SWAPCHAIN_MAINTENANCE_1_FEATURES_KHR };


		explicit DesiredFeatures(bool presentation, bool includeUnifiedImageLayouts, bool includeAddressCommands)
		{
			core.pNext = &vulkan11;
			vulkan11.pNext = &vulkan12;
			vulkan12.pNext = &vulkan13;
			vulkan13.pNext = &vulkan14;
			vulkan14.pNext = &descriptorHeap;
			descriptorHeap.pNext = includeAddressCommands ? static_cast<void*>(&addressCommands) : static_cast<void*>(&untypedPointers);
			addressCommands.pNext = &untypedPointers;
			untypedPointers.pNext = includeUnifiedImageLayouts ? static_cast<void*>(&unifiedImageLayouts) : static_cast<void*>(&meshShader);
			unifiedImageLayouts.pNext = &meshShader;
			meshShader.pNext = presentation ? &swapchainMaintenance1 : nullptr;
		}
	};

	// Filled in when inspecting potential devices
	struct Candidate
	{
		VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
		uint32_t queueFamilies[queueTypeCount]{};
		uint32_t queueCounts[queueTypeCount]{};
		bool unifiedImageLayouts		= false;
		bool addressCommands			= false;
		bool imageCubeArray				= false;
		bool textureCompressionBc		= false;
		bool textureCompressionAstc		= false;
		bool textureCompressionEtc2		= false;
		bool storageInputOutput16		= false;
		bool timestamps[queueTypeCount]{};

		uint32x3 copyTextureGranularity = { .x = 1, .y = 1, .z = 1 }; // Used by transfer queue

		VkPhysicalDeviceDescriptorHeapPropertiesEXT heapProperties{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_HEAP_PROPERTIES_EXT };
		VkPhysicalDeviceVulkan12Properties vulkan12Properties{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_PROPERTIES };
		VkPhysicalDeviceProperties2 properties{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2 }; // NOTE: in the orignial noapi this uses prop1 instead of prop2
		VkPhysicalDeviceMemoryProperties2 memoryProperties{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2 }; // Same here
	};

	// Checks if device we are inspecting has required features
	VkResult inspectDevice(VkPhysicalDevice device, const DesiredDevice& desc, const char* maintenanceExtensionName, VkSurfaceKHR surface, Candidate& output)
	{
		std::vector<VkExtensionProperties> supportedDeviceExtensions;
		std::vector<const char*> enabledDeviceExtensions;
		if (getDeviceInstanceExtensions(supportedDeviceExtensions, device) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to get device instance extensnions");
		}

		// Check for various required device extensions. Also check for the selected maintenance extension name
		const bool unifiedImageLayoutsExtension = hasName(supportedDeviceExtensions, VK_KHR_UNIFIED_IMAGE_LAYOUTS_EXTENSION_NAME);
		const bool addressCommandsExtension = hasName(supportedDeviceExtensions, VK_KHR_DEVICE_ADDRESS_COMMANDS_EXTENSION_NAME);
		for (const char* name : requiredDeviceExtensions)
		{
			if (hasName(supportedDeviceExtensions, name))
			{
				enabledDeviceExtensions.push_back(name);
			}
		}
		if (enabledDeviceExtensions.size() < sizeof(requiredDeviceExtensions) / sizeof(requiredDeviceExtensions[0]))
		{
			return VK_INCOMPLETE;
		}
		// Find which swapchain maintence we are using.
		// NOTE: noapi checks for which surface maintenance(EXT or KHR). However, I feel like that is unnecessary.
		// But just in case something breaks add that check here
		if (hasName(supportedDeviceExtensions, VK_KHR_SWAPCHAIN_MAINTENANCE_1_EXTENSION_NAME))
		{
			enabledDeviceExtensions.push_back(VK_KHR_SWAPCHAIN_MAINTENANCE_1_EXTENSION_NAME);
		}
		else if (hasName(supportedDeviceExtensions, VK_EXT_SWAPCHAIN_MAINTENANCE_1_EXTENSION_NAME))
		{
			enabledDeviceExtensions.push_back(VK_EXT_SWAPCHAIN_MAINTENANCE_1_EXTENSION_NAME);
		}
		else
		{
			return VK_INCOMPLETE;
		}

		// Get properties, including heap and memory properties
		Candidate result
		{
			.physicalDevice = device,
		};
		vkGetPhysicalDeviceProperties2(device, &result.properties);
		const char* deviceName = result.properties.properties.deviceName;
		result.heapProperties.pNext = nullptr;
		result.vulkan12Properties.pNext = nullptr;
		vkGetPhysicalDeviceMemoryProperties2(device, &result.memoryProperties);
		// TODO: add check for cpu visibile memory here

		// Check if we have the desired features
		DesiredFeatures features(true, unifiedImageLayoutsExtension, addressCommandsExtension);
		vkGetPhysicalDeviceFeatures2(device, &features.core);
		const bool requiredFeatures =
			features.core.features.shaderInt16 == VK_TRUE &&
			features.core.features.samplerAnisotropy == VK_TRUE &&
			features.core.features.depthBiasClamp == VK_TRUE &&
			features.core.features.independentBlend == VK_TRUE &&
			features.core.features.fragmentStoresAndAtomics == VK_TRUE &&
			features.core.features.vertexPipelineStoresAndAtomics == VK_TRUE &&
			features.core.features.shaderStorageImageReadWithoutFormat == VK_TRUE &&
			features.core.features.shaderStorageImageWriteWithoutFormat == VK_TRUE &&
			features.core.features.multiDrawIndirect == VK_TRUE &&
			features.core.features.drawIndirectFirstInstance == VK_TRUE &&
			features.vulkan11.storageBuffer16BitAccess == VK_TRUE &&
			features.vulkan11.storagePushConstant16 == VK_TRUE &&
			features.vulkan11.shaderDrawParameters == VK_TRUE &&
			features.vulkan12.shaderFloat16 == VK_TRUE &&
			features.vulkan12.scalarBlockLayout == VK_TRUE &&
			features.vulkan12.bufferDeviceAddress == VK_TRUE &&
			features.vulkan12.timelineSemaphore == VK_TRUE &&
			features.vulkan13.synchronization2 == VK_TRUE &&
			features.vulkan13.dynamicRendering == VK_TRUE &&
			features.vulkan13.maintenance4 == VK_TRUE &&
			features.vulkan14.maintenance5 == VK_TRUE &&
			features.descriptorHeap.descriptorHeap == VK_TRUE &&
			features.untypedPointers.shaderUntypedPointers == VK_TRUE &&
			features.meshShader.meshShader == VK_TRUE &&
			(features.core.features.textureCompressionBC == VK_TRUE ||
				features.core.features.textureCompressionASTC_LDR == VK_TRUE) &&
			(features.swapchainMaintenance1.swapchainMaintenance1 == VK_TRUE); // noapi shecks for either if a surface is null. But we are always going to be presenting so we dont need that check
		if (!requiredFeatures)
		{
			return VK_INCOMPLETE;
		}
		// Optional check to see if we have these features
		result.imageCubeArray =			features.core.features.imageCubeArray == VK_TRUE;
		result.textureCompressionBc =	features.core.features.textureCompressionBC == VK_TRUE;
		result.textureCompressionAstc = features.core.features.textureCompressionASTC_LDR == VK_TRUE;
		result.textureCompressionEtc2 = features.core.features.textureCompressionETC2 == VK_TRUE;
		result.storageInputOutput16 =	features.vulkan11.storageInputOutput16 == VK_TRUE;

		// TODO: noapi adds checks for these. Not sure if necessary but if something goes wrong with address commands/image layouts fix here
		result.unifiedImageLayouts = unifiedImageLayoutsExtension && features.unifiedImageLayouts.unifiedImageLayouts == VK_TRUE;
		result.addressCommands = addressCommandsExtension && features.addressCommands.deviceAddressCommands == VK_TRUE;

		uint32_t availableQueueFamilies = 0;
		vkGetPhysicalDeviceQueueFamilyProperties2(device, &availableQueueFamilies, nullptr);
		if (availableQueueFamilies > maxQueueFamilies)
		{
			return VK_INCOMPLETE;
		}

		std::vector<VkQueueFamilyProperties2> queueFamilyProperties2(availableQueueFamilies);
		for (uint32_t index = 0; index < availableQueueFamilies; ++index)
		{
			queueFamilyProperties2.at(index).sType = VK_STRUCTURE_TYPE_QUEUE_FAMILY_PROPERTIES_2;
		}
		vkGetPhysicalDeviceQueueFamilyProperties2(device, &availableQueueFamilies, queueFamilyProperties2.data());
		// Now get queue information from each family
		for (uint32_t index = 0; index < availableQueueFamilies; ++index)
		{
			if (queueFamilyProperties2.at(index).queueFamilyProperties.queueCount == 0) continue;
			// Get family type and and set the variable
			const VkQueueFlags flags = queueFamilyProperties2.at(index).queueFamilyProperties.queueFlags;
			uint32_t queueType = 0;
			if (flags & VK_QUEUE_GRAPHICS_BIT)
			{
				if (!(flags & VK_QUEUE_COMPUTE_BIT)) continue;
			}
			else if (flags & VK_QUEUE_COMPUTE_BIT)
			{
				queueType = 1;
			}
			// Essentially disallows Video Decode/Encode. Keeps normal transfer bit
			else if ((flags & VK_QUEUE_TRANSFER_BIT) &&
				!(flags & ~(VK_QUEUE_TRANSFER_BIT | VK_QUEUE_SPARSE_BINDING_BIT | VK_QUEUE_PROTECTED_BIT)))
			{
				queueType = 2;
			}
			else
			{
				continue;
			}
			if (result.queueCounts[queueType] != 0) continue;
			// Check that present family(usually first family) supports present
			VkBool32 presentSupported = true;
			if (surface && queueType == 0)
			{
				VkResult presentResult = vkGetPhysicalDeviceSurfaceSupportKHR(device, index, surface, &presentSupported);
				if (presentResult != VK_SUCCESS)
				{
					return VK_INCOMPLETE;
				}
			}
			if (presentSupported == VK_TRUE)
			{
				result.queueFamilies[queueType] = index;
				result.queueCounts[queueType] = queueFamilyProperties2.at(index).queueFamilyProperties.queueCount;
				result.timestamps[queueType] = desc.timestampQueryCount != 0 && queueFamilyProperties2.at(index).queueFamilyProperties.timestampValidBits == 64 && features.vulkan12.hostQueryReset;
				if (queueType == 2)
				{
					result.copyTextureGranularity =
					{
						.x = queueFamilyProperties2.at(index).queueFamilyProperties.minImageTransferGranularity.width,
						.y = queueFamilyProperties2.at(index).queueFamilyProperties.minImageTransferGranularity.height,
						.z = queueFamilyProperties2.at(index).queueFamilyProperties.minImageTransferGranularity.depth,
					};
				}
			}
		}
		if (result.queueCounts[0] == 0)
		{
			return VK_INCOMPLETE;
		}

		output = result;
		return VK_SUCCESS;
	}

	bool hasName(const std::vector<VkExtensionProperties>& extensionArr, const char* nameCheck)
	{
		for (const VkExtensionProperties& extension : extensionArr) {
			if (strcmp(nameCheck, extension.extensionName) == 0)
			{
				return true;
			}
			// Secondary check to catch if strcmp fails
			if (nameCheck == extension.extensionName)
			{
				return true;
			}
		}
		return false;
	}
	bool hasName(const std::vector<const char*>& extensionArr, const char* nameCheck)
	{
		for (const char* extension : extensionArr) {
			if (strcmp(nameCheck, extension) == 0)
			{
				return true;
			}
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
		uint32_t count = 512;
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

	DeviceInit createDevice(const DesiredDevice desc)
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
		if (hasName(supportedInstanceExtensions, VK_KHR_SURFACE_MAINTENANCE_1_EXTENSION_NAME) == true)
		{
			enabledInstanceExtensions.push_back(VK_KHR_SURFACE_MAINTENANCE_1_EXTENSION_NAME);
			surfaceMaintenanceFound = true;
			foundMaintenanceName = VK_KHR_SURFACE_MAINTENANCE_1_EXTENSION_NAME;
		}
		else if (hasName(supportedInstanceExtensions, VK_EXT_SURFACE_MAINTENANCE_1_EXTENSION_NAME) == true)
		{
			enabledInstanceExtensions.push_back(VK_EXT_SURFACE_MAINTENANCE_1_EXTENSION_NAME);
			surfaceMaintenanceFound = true;
			foundMaintenanceName = VK_EXT_SURFACE_MAINTENANCE_1_EXTENSION_NAME;
		}
		if (!surfaceMaintenanceFound)
		{
			throw std::runtime_error("Surface Maintenance Extension not found! May not be supported on device");
		}
		// NOTE: always use pointers or references when using vectors as if you dont it will self-modify its values and mess up your result
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
		if (addedExtensions < sizeof(requiredInstanceExtensions) / sizeof(requiredInstanceExtensions[0]))// TODO: remove this hardcoded value later by turning required extensions into a vector
		{
			throw std::runtime_error("Failed to get all required extensions, check debugger for what is mising.");
		}

		uint32_t glfwCount = 0;
		const char** glfwExtensionsRequired = glfwGetRequiredInstanceExtensions(&glfwCount);
		// After checking that we don't already have that extension, add what GLFW needs
		// This probably only applies to VK_KHR_Surface, and I dont think it matters too much but just in case
		for (uint32_t index = 0; index < glfwCount; ++index)
		{
			const char* extension = glfwExtensionsRequired[index];
			if (!hasName(enabledInstanceExtensions, extension))
			{
				enabledInstanceExtensions.push_back(extension);
			}
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

		// Create the GLFW/Vulkan surface before passing it to the inspectDevice method as it needs to be NOT null
		VkResult glfwResult = glfwCreateWindowSurface(device->instance, desc.window, nullptr, &device->surface);
		if (glfwResult != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create window surface for GLFW.");
		}

		// Get available physical devices, its features and properties, and queue information
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
			if (inspectDevice(physicalDevice, desc, foundMaintenanceName, device->surface, candidate) == VK_SUCCESS)
			{
				selects = candidate;
				break;
			}
		}

		// Set device properties


		// Get device info and create device
		//VkDeviceCreateInfo deviceInfo
		//{
		//	.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,

		//}

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