#include "RenderCore.h"


namespace gpu {

	#ifdef NDEBUG
		constexpr bool enableValidationLayers = false;
	#else
		constexpr bool enableValidationLayers = true;
	#endif

	static constexpr int width = 1280;
	static constexpr int height = 720;

	static uint32_t frameIndex = 0;
	static const uint32_t maxColorAttachments = 1;
	const uint32_t maxSwapchainImages = 3;
	static VkPresentModeKHR presentMode = VkPresentModeKHR::VK_PRESENT_MODE_FIFO_KHR;
	GLFWwindow* window;

	// Device property variables
	// NOTE: Vectors may be better but at this point not really necessary
	const uint32_t maxDeviceExtensions = 512;
	const uint32_t maxInstanceExtensions = 512;
	const uint32_t maxPhysicalDevices  = 5; // Really shouldn't be higher than 2 (in-built GPU, discreet GPU, maybe a second discreet GPU)
	// Want to see your GPU capabilities? Use this website:
	// https://vulkan.gpuinfo.org/
	const uint32_t maxQueueFamilies = 64;
	const uint32_t queueTypeCount = 3; // The 3 queues: Graphics, Compute, Transfer.

	constexpr VkDescriptorSetAndBindingMappingEXT rootMapping{
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_AND_BINDING_MAPPING_EXT,
		.bindingCount = 1,
		.resourceMask = VK_SPIRV_RESOURCE_TYPE_UNIFORM_BUFFER_BIT_EXT | VK_SPIRV_RESOURCE_TYPE_READ_ONLY_STORAGE_BUFFER_BIT_EXT,
		.source = VK_DESCRIPTOR_MAPPING_SOURCE_PUSH_ADDRESS_EXT,
		.sourceData = { .pushAddressOffset = 0 },
	};
	

	struct GpuRange
	{

	};

	struct GpuHeap
	{

	};

	// What we want from our GPU
	struct DesiredDevice
	{
		uint32_t desiredGraphicsQueues = 1; // Graphics(present) must be at least 1
		uint32_t desiredComputeQueues = 0;
		uint32_t desiredTransferQueues = 0;

		uint32_t desiredSwapchainImageCount = 2;
		uint32_t timeStampQueryCount = 256;

		VkColorSpaceKHR colorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
	};

	// Device, or the actual GPU we are interacting with
	struct Device
	{
		std::string deviceName = "";
		VkDevice device = VK_NULL_HANDLE;
		VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
		VkInstance vkInstance = VK_NULL_HANDLE;
		VkSurfaceKHR surface = VK_NULL_HANDLE;

		// TODO: Research why noapi doesnt define the .sType for these 2 variables under this comment
		VkPhysicalDeviceProperties2 deviceProps{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2 };
		VkPhysicalDeviceMemoryProperties2 deviceMemProps{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2 };
		VkPhysicalDeviceDescriptorHeapPropertiesEXT deviceHeapProps{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_HEAP_PROPERTIES_EXT };
		DeviceCapabilities deviceCaps;

		Queue* queues = nullptr;
		uint32_t queueCount = 0;
		uint32_t queueFamilyCount = 0;
		uint32_t timestampQueryCount = 0;
		std::vector<uint32_t> queueFamilies;
		uint32_t textureMemoryType = VK_MAX_MEMORY_TYPES;
		uint64_t textureHeapAlignment = 16;
		DeviceCapabilities capabilities;

		Swapchain* swapChain = nullptr;
		Swapchain* acquiredSwapChain = nullptr;
	};

	// The queues that exist on the GPU that handle sending application info into the GPU
	struct Queue
	{
		VkQueue queue = VK_NULL_HANDLE;
		uint32_t familyIndex = 0;
		size_t commandSubmitCap = 0;
	};

	struct DeviceCapabilities
	{
		// Queue Capabilities
		uint32_t queueCount = 0;
		uint32_t queueGeneralCount = 0;
		uint32_t queueComputeCount = 0;
		uint32_t queueTransferCount = 0; //  Also called copy queue in no_api
		uint32_t timestampQueryCount = 0;
		// TODO: add compression texture stuff here if we go that route

		bool textureCompressionBC = false;
		bool textureCompressionASTC = false;
		bool storageInputOutput16 = false;
		bool indirectMeshDraw = false;
	};

	struct GpuTextureSizeAlign
	{
		size_t size;
		size_t align;
	};

	// Series of images of rendered images that will most-likely be presented
	struct Swapchain
	{
		Device* state = nullptr;
		VkSwapchainKHR handle = VK_NULL_HANDLE;

		VkImage images[maxSwapchainImages]{};

		//present::RenderView renderViews[maxSwapchainImages]{};
		uint32_t imageIndex = 0;
		uint32_t width = 0;
		uint32_t height = 0;
		VkColorSpaceKHR colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR; // Default image format that will be used
		VkSurfaceTransformFlagBitsKHR transform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
		VkCompositeAlphaFlagBitsKHR compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
		CommandBuffer* transitionCommands = nullptr;
		bool acquired = false;
		bool recreate_required = false; //  Happens when resolution changes/minimized or maximized.

		PresentContext* presentContext = nullptr;
	};

	// Series of commands for the GPU to execute
	struct CommandBuffer 
	{
		Device* state = nullptr;
		CommandBuffer* next = nullptr;
		VkCommandPool commandPool = VK_NULL_HANDLE;
		VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
		VkCommandBuffer epilogue = VK_NULL_HANDLE;
		VkQueryPool timestampPool = VK_NULL_HANDLE;

		Swapchain* swapchain = nullptr;
		bool submitted = false;
		bool has_epilogue = false;

		uint32_t timestampCount = 0;
		uint64_t** timestampDestinations = nullptr;
		uint64_t* timestampResults = nullptr;
	};

	// Way to organize buffers into pools
	struct CommandPool
	{
		Device* state = nullptr;
		VkCommandPool commandPool = VK_NULL_HANDLE;
		CommandBuffer* first = nullptr;
		CommandBuffer* last = nullptr;
		CommandBuffer* nextBuffer = nullptr;
		bool timestamps = false;
	};

	struct PresentContext
	{
		VkSemaphore acquired = VK_NULL_HANDLE;
		VkSemaphore rendered = VK_NULL_HANDLE;
		VkFence presented = VK_NULL_HANDLE;
		VkSwapchainKHR swapchain = VK_NULL_HANDLE;
		bool presentPending = false;
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

	// Setup methods

	// Score a candidate physical device, highest scorer is picked. If any part fails, return 0 as the device being checked is unsuitable
	// Unlike in noapi, we are going to run with the assumption that we want swapchainMaintenance1
	// Score mainly determined by imageExtent
	uint32_t scoreDevice(VkPhysicalDevice device, VkSurfaceKHR surface)
	{

		constexpr const char* required_extensions[]{
			VK_EXT_DESCRIPTOR_HEAP_EXTENSION_NAME,
			VK_KHR_SHADER_UNTYPED_POINTERS_EXTENSION_NAME,
			VK_EXT_MESH_SHADER_EXTENSION_NAME,
			VK_KHR_SWAPCHAIN_EXTENSION_NAME
		};

		// Variables for tracking device capabilities
		uint32_t score = 0;
		uint32_t extensionCount = 0;
		uint32_t queueCount = 0;
		bool hasImageLayourExtension = false;
		bool hasAddressCommandsExtension = false;
		std::vector<VkExtensionProperties> extensionProperties;
		std::vector<VkQueueFamilyProperties2> queueFamilyProperties;
		VkPhysicalDeviceProperties2 deviceProperties2{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2 };

		// Find specific extensions


		DesiredFeatures features(surface != VK_NULL_HANDLE, hasImageLayourExtension, hasAddressCommandsExtension);

		// Get extensions
		VkResult extensionResult = vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, extensionProperties.data());
		if (extensionResult != VK_SUCCESS)
		{
			return 0;
		}

		// Get device properties and iterate to find all required properties
		vkGetPhysicalDeviceProperties2(device, &deviceProperties2);
		vkGetPhysicalDeviceFeatures2(device, &features.core);
		

		// If any of these fail, device is not suitable and will score 0
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
			(!surface || features.swapchainMaintenance1.swapchainMaintenance1 == VK_TRUE);

		if (!requiredFeatures)
		{
			std::cout << "This device lacked the required features" << std::endl;
			return 0;
		}

		// Determine queue count and families, check that needed queues exist
		bool presentQueueFound = false;
		vkGetPhysicalDeviceQueueFamilyProperties2(device, &queueCount, queueFamilyProperties.data());
		for (uint32_t index = 0; index < queueCount; ++index)
		{
			if (queueFamilyProperties.at(index).queueFamilyProperties.queueCount == 0)
				continue;

			const VkFlags queueFlag = queueFamilyProperties.at(index).queueFamilyProperties.queueFlags;
			uint32_t queueType = 0; // 1 = graphics, 2 = compute, 3 = transfer

			if (queueFlag == VK_QUEUE_GRAPHICS_BIT)
			{
				queueType = 1;
			}
			else if (queueFlag == VK_QUEUE_COMPUTE_BIT)
			{
				queueType = 2;
			}
			else if (queueFlag == VK_QUEUE_TRANSFER_BIT)
			{
				queueType = 3;
			}
			else
			{
				continue;
			}
			// Check surface presentation capability with queue index
			if (surface && queueType == 1)
			{
				VkBool32 presentSupported = VK_TRUE;
				vkGetPhysicalDeviceSurfaceSupportKHR(device, index, surface, &presentSupported);
				if (presentSupported == VK_FALSE)
				{
					return 0;
				}
				presentQueueFound = true;
			}
		}

		if (!presentQueueFound)
		{
			return 0;
		}

		// The main determinant of score
		score += deviceProperties2.properties.limits.maxImageDimension2D;

		return score;

	}

	// Find physical device
	// Refer to inspect_candidate in no_api
	void pickPhysicalDevice(VkInstance instance, Device* deviceInfo)
	{
		uint32_t deviceCount = 0;
		std::vector<VkPhysicalDevice> devices;
		uint32_t currentBestScore = 0;
		VkPhysicalDevice bestDevice;
		//std::vector<VkPhysicalDevice> deviceList;
		VkResult physicalDeviceResult = vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());


		if (physicalDeviceResult != VK_SUCCESS)
		{
			throw std::runtime_error("No GPU found in system!");
		}
		
		// Iterate each device and run check
		for (const auto& device : devices)
		{
			uint32_t currentScore = scoreDevice(device, deviceInfo->surface);
			if (currentScore > currentBestScore) // Stupid check but in theory should work 99% of the time
			{
				bestDevice = device;
				currentBestScore = currentScore;
			}
		}

		if (currentBestScore == 0)
		{
			throw std::runtime_error("No suitable GPU found! All scored 0!");
		}
		deviceInfo->physicalDevice = bestDevice;
	}

	// TODO: add if (DEBUG) condition for the validation layer
	// Fill in empty Device struct from trying to initialize Framework
	bool createDevice(Device* device, const DesiredDevice& desiredDevice)
	{
		// Get extensions
		const bool presentation = true;
		uint32_t extensionCount = 0;
		std::vector<VkExtensionProperties> extensions;
		std::vector<const char*> enabledInstanceExtensions;
		std::vector<const char*> enabledLayers;
		uint32_t layerCount = 0;
		vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data());

		// Create context and instance of VK
		const VkApplicationInfo appInfo
		{
			.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
			.pApplicationName  = "Framework",
			.applicationVersion = VK_MAKE_API_VERSION(1, 0, 0, 0),
			.pEngineName  = "N/A",
			.engineVersion = VK_MAKE_API_VERSION(1, 0, 0, 0),
			.apiVersion = VK_API_VERSION_1_4
		};
		

		// TODO: enable instance extensions here
		if (presentation)
		{
			enabledInstanceExtensions.push_back(VK_EXT_SWAPCHAIN_COLOR_SPACE_EXTENSION_NAME);
			enabledInstanceExtensions.push_back(VK_KHR_SURFACE_EXTENSION_NAME);
			enabledInstanceExtensions.push_back(VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME);
			enabledInstanceExtensions.push_back(VK_KHR_SURFACE_MAINTENANCE_1_EXTENSION_NAME);
			enabledInstanceExtensions.push_back(VK_EXT_SURFACE_MAINTENANCE_1_EXTENSION_NAME);
		}

		// Enable validation layer if debugging
		if (enableValidationLayers)
		{
			enabledLayers[layerCount++] = "VK_LAYER_KHRONOS_validation";
		}

		const VkInstanceCreateInfo instanceInfo
		{
			.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
			.pApplicationInfo = &appInfo,
			.enabledLayerCount = enabledLayers.size(),
			.ppEnabledLayerNames = enabledLayers.data(),
			.enabledExtensionCount = enabledInstanceExtensions.size(),
			.ppEnabledExtensionNames = enabledInstanceExtensions.data()
		};
		VkResult instanceResult = vkCreateInstance(&instanceInfo, nullptr, &device->vkInstance);

		if (instanceResult != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create VK Instance!");
		}

		// Find physical device to use and begin populating the rest of the Device* device param
		// TODO: Get physicalDevice Heap properties
		pickPhysicalDevice(device->vkInstance, device);
		vkGetPhysicalDeviceProperties2(device->physicalDevice, &device->deviceProps);
		vkGetPhysicalDeviceMemoryProperties2(device->physicalDevice, &device->deviceMemProps);

		
		

		DesiredFeatures enabledFeatures(presentation, true, true); // TODO: remove magic boolean
		enabledFeatures.core.features.imageCubeArray = VK_TRUE;
		enabledFeatures.core.features.samplerAnisotropy = VK_TRUE;
		enabledFeatures.core.features.shaderInt16 = VK_TRUE;
		enabledFeatures.core.features.depthBiasClamp = VK_TRUE;
		enabledFeatures.core.features.independentBlend = VK_TRUE;
		enabledFeatures.core.features.textureCompressionBC = VK_TRUE;
		enabledFeatures.core.features.textureCompressionASTC_LDR = VK_TRUE;
		enabledFeatures.core.features.textureCompressionETC2 = VK_TRUE;
		enabledFeatures.core.features.fragmentStoresAndAtomics = VK_TRUE;
		enabledFeatures.core.features.vertexPipelineStoresAndAtomics = VK_TRUE;
		enabledFeatures.core.features.shaderStorageImageReadWithoutFormat = VK_TRUE;
		enabledFeatures.core.features.shaderStorageImageWriteWithoutFormat = VK_TRUE;
		enabledFeatures.core.features.multiDrawIndirect = VK_TRUE;
		enabledFeatures.core.features.drawIndirectFirstInstance = VK_TRUE;
		enabledFeatures.vulkan11.storageBuffer16BitAccess = VK_TRUE;
		enabledFeatures.vulkan11.storagePushConstant16 = VK_TRUE;
		enabledFeatures.vulkan11.storageInputOutput16 = VK_TRUE;
		enabledFeatures.vulkan11.shaderDrawParameters = VK_TRUE;
		enabledFeatures.vulkan12.shaderFloat16 = VK_TRUE;
		enabledFeatures.vulkan12.scalarBlockLayout = VK_TRUE;
		enabledFeatures.vulkan12.timelineSemaphore = VK_TRUE;
		enabledFeatures.vulkan12.bufferDeviceAddress = VK_TRUE;
		enabledFeatures.vulkan13.synchronization2 = VK_TRUE;
		enabledFeatures.vulkan13.dynamicRendering = VK_TRUE;
		enabledFeatures.vulkan13.maintenance4 = VK_TRUE;
		enabledFeatures.vulkan14.maintenance5 = VK_TRUE;
		enabledFeatures.descriptorHeap.descriptorHeap = VK_TRUE;
		enabledFeatures.addressCommands.deviceAddressCommands = VK_TRUE;
		enabledFeatures.untypedPointers.shaderUntypedPointers = VK_TRUE;
		enabledFeatures.unifiedImageLayouts.unifiedImageLayouts = VK_TRUE; // TODO: readd boolean operation from noapi here
		enabledFeatures.meshShader.taskShader = VK_TRUE;
		enabledFeatures.meshShader.meshShader = VK_TRUE;
		enabledFeatures.swapchainMaintenance1.swapchainMaintenance1 = VK_TRUE;

		// Queue selection
		uint32_t queueCount = 0;
		std::vector<VkQueueFamilyProperties2> queueFamilyProperties; // Gets each queue family, not each queue important detail
		vkGetPhysicalDeviceQueueFamilyProperties2(device->physicalDevice, &queueCount, queueFamilyProperties.data());
		// For each found queue family, iterate and get desired queue amount, and assign it to device queue struct
		const uint32_t requestedQueueCounts[]{ desiredDevice.desiredGraphicsQueues, desiredDevice.desiredComputeQueues, desiredDevice.desiredTransferQueues };
		uint32_t queueCounts[queueTypeCount];
		uint32_t queueFamilyIndex[maxQueueFamilies];
		bool desiredTimestamps[queueTypeCount];
		for (uint32_t queueType = 0; queueType < queueCount; ++queueType)
		{
			// TODO: Possibly incorrect logic, debug for sure
			// NOTE: assign queue counts from requestedQueues[] if lower than the queues in the family we are looking at, otherwise assign queue counts from family if requestedQueues[] is larger
			// NOTE: noapi uses the queue_counts found in their own inspect_candidate. This should be roughly the same though as that also finds ALL of the queues available
			queueCounts[queueType] = requestedQueueCounts[queueType] < queueFamilyProperties.at(queueType).queueFamilyProperties.queueCount ? requestedQueueCounts[queueType] : queueFamilyProperties.at(queueType).queueFamilyProperties.queueCount;
			device->queueCount += queueCounts[queueType];
		}

		// Find queue family indexes and those that supports presentation
		// Similar to how we check for presentation support in scoreDevice
		for (int index = 0; index < queueCount; ++index)
		{
			uint32_t queueType = 0;
			const VkQueueFlags flags = queueFamilyProperties.at(index).queueFamilyProperties.queueFlags;
			VkBool32 presentSupport = true;
			if (flags & VK_QUEUE_GRAPHICS_BIT)
			{
				if (!(flags & VK_QUEUE_COMPUTE_BIT)) continue;
			}
			else if (flags & VK_QUEUE_COMPUTE_BIT)
			{
				queueType = 1;
			}
			else if ((flags & VK_QUEUE_TRANSFER_BIT) &&
				!(flags & ~(VK_QUEUE_TRANSFER_BIT | VK_QUEUE_SPARSE_BINDING_BIT | VK_QUEUE_PROTECTED_BIT)))
			{
				queueType = 2;
			}
			// organize each queue of that type with its index, check that our graphics/present queue family(commonly family 0) supports presentation
			if (device->surface && queueType == 0) // Since family 0 is the one that contains present Queue, we check this mostly
			{
				
				vkGetPhysicalDeviceSurfaceSupportKHR(device->physicalDevice, index, device->surface, &presentSupport);
				if (presentSupport == false)
				{
					std::cout << "Family index: " << index << std::endl;
					throw std::runtime_error("Surface/queue index does not support presentation");
				}
				
			}
			if (presentSupport == true)
			{
				queueFamilyIndex[queueType] = index;
				desiredTimestamps[queueType] = desiredDevice.timeStampQueryCount != 0 && queueFamilyProperties.at(index).queueFamilyProperties.timestampValidBits == 64 && enabledFeatures.vulkan12.hostQueryReset;
			}
		}

		enabledFeatures.vulkan12.hostQueryReset = desiredTimestamps[0] || desiredTimestamps[1] || desiredTimestamps[2];

		// Iterate through each queue family/type and create the queues we want
		device->queues = new Queue[queueCount];
		VkDeviceQueueCreateInfo queueInfos[queueTypeCount]{};
		uint32_t firstQueue = 0;
		float* queuePriorities = new float[device->queueCount];
		// Assign queue priorities
		for (uint32_t index = 0; index < device->queueCount; ++index)
		{
			queuePriorities[index] = 1.0f;
		}
		for (uint32_t queueType = 0; queueType < queueTypeCount; ++queueType)
		{
			if (queueCounts[queueType] == 0)
			{
				std::cout << "Queue family of index: " << queueType << ", doesn't have any queues assigned, ensure this isn't our graphics/present queue!" << std::endl;
				continue;
			}
			device->queueFamilies[device->queueFamilyCount] = queueFamilyProperties.at(queueType).queueFamilyProperties.queueCount; // TODO: this is wrong, I think
			queueInfos[device->queueFamilyCount++] =
			{
				.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
				.queueFamilyIndex = queueFamilyIndex[queueType],
				.queueCount = queueCounts[queueType],
				.pQueuePriorities = queuePriorities + firstQueue,
			};
			// Very strange looking piece of logic for this
			// This would mean after queue family 0, the next family takes priority?
			// So research if this is what we want
			firstQueue += queueCounts[queueType];
		}

		// Enable device extensions
		std::vector<const char*> enableDeviceExentsions;
		enableDeviceExentsions.push_back(VK_EXT_DESCRIPTOR_HEAP_EXTENSION_NAME);
		if (enabledFeatures.addressCommands.deviceAddressCommands == VK_TRUE) { enableDeviceExentsions.push_back(VK_KHR_DEVICE_ADDRESS_COMMANDS_EXTENSION_NAME); };
		enableDeviceExentsions.push_back(VK_KHR_SHADER_UNTYPED_POINTERS_EXTENSION_NAME);
		if (enabledFeatures.unifiedImageLayouts.unifiedImageLayouts == VK_TRUE) { enableDeviceExentsions.push_back(VK_KHR_UNIFIED_IMAGE_LAYOUTS_EXTENSION_NAME); };
		enableDeviceExentsions.push_back(VK_EXT_MESH_SHADER_EXTENSION_NAME);
		#if defined(_WIN32)
				if (presentation)
				{
					enableDeviceExentsions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
					enableDeviceExentsions.push_back(VK_EXT_SWAPCHAIN_MAINTENANCE_1_EXTENSION_NAME);
				}
		#endif

		const VkDeviceCreateInfo deviceInfo
		{
			.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
			.pNext = &enabledFeatures.core,
			.queueCreateInfoCount = device->queueFamilyCount,
			.pQueueCreateInfos = queueInfos,
			.enabledExtensionCount = enableDeviceExentsions.size(),
			.ppEnabledExtensionNames = enableDeviceExentsions.data()
		};
		VkResult deviceResult = vkCreateDevice(device->physicalDevice, &deviceInfo, nullptr, &device->device);

		if (deviceResult != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create device");
		}


		// Assign variables to device struct
		device->deviceName = "UNKOWN";
		device->queueCount = queueCount;
		device->deviceCaps.queueGeneralCount = queueCounts[0];
		device->deviceCaps.queueComputeCount = queueCounts[1];
		device->deviceCaps.queueTransferCount = queueCounts[2];
		device->deviceCaps.indirectMeshDraw = true;
		device->deviceCaps.storageInputOutput16 = true;

		// Create swapchain
		device->swapChain = new Swapchain;
		device->swapChain->state = device;
		//device->swapChain = device;


		return true;
	}

	// GPU memory methods
	void* gpuMalloc(size_t bytes)
	{
		return nullptr;
	}
	void* gpuMalloc(size_t bytes, size_t alignment)
	{
		return nullptr;
	}
	void* gpuHostToDevicePointer(void* ptr)
	{
		return nullptr;
	}
	void gpuMemCopy()
	{
	}
	void gpuFree(void* ptr)
	{
	}
	void gpuSetActiveTextureHeapPtr()
	{
	}

	void draw(CommandBuffer* commands, const void* root, uint32_t vertexCount)
	{
		vkCmdDraw(commands->commandBuffer, vertexCount, 0, 0, 0);
	}


	void resetCommandPool(CommandPool* pool)
	{
		for (CommandBuffer* currBuffer = pool->first; currBuffer; currBuffer = currBuffer->next)
		{
			currBuffer->submitted = false;
			currBuffer->timestampCount = 0;
		}
		VkResult resetResult = vkResetCommandPool(pool->state->device, pool->commandPool, 0);
		if (resetResult != VK_SUCCESS)
		{
			std::cout << "Failed to reset command pool. Error code: " << resetResult << std::endl;
		}
		pool->nextBuffer = pool->first;
	}

	CommandBuffer* beginCommands(CommandPool* pool)
	{
		CommandBuffer* commands = pool->nextBuffer;
		if (commands)
		{
			pool->nextBuffer = commands->next;
		}
		else
		{
			commands = new CommandBuffer{.state = pool->state, .commandPool = pool->commandPool};
			const VkCommandBufferAllocateInfo allocateInfo
			{
				.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
				.commandPool = pool->commandPool,
				.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
				.commandBufferCount = 1,
			};
			VkResult allocationResult = vkAllocateCommandBuffers(pool->state->device, &allocateInfo, &commands->commandBuffer);
			if (allocationResult != VK_SUCCESS)
			{
				delete commands;
				return nullptr;
			}
			if (pool->timestamps)
			{
				const VkQueryPoolCreateInfo queryPoolInfo
				{
					.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
					.queryType = VK_QUERY_TYPE_TIMESTAMP,
					.queryCount = pool->state->timestampQueryCount,
				};
				VkResult queryPoolResult = vkCreateQueryPool(pool->state->device, &queryPoolInfo, nullptr, &commands->timestampPool);
				if (queryPoolResult != VK_SUCCESS)
				{
					vkFreeCommandBuffers(pool->state->device, pool->commandPool, 1, &commands->commandBuffer);
					delete commands;
					return nullptr;
				}
				commands->timestampDestinations = static_cast<uint64_t**>(malloc(sizeof(uint64_t*) * pool->state->timestampQueryCount));
				commands->timestampResults = static_cast<uint64_t*>(malloc(sizeof(uint64_t) * pool->state->timestampQueryCount));
			}
			if (pool->last)
			{
				pool->last->next = commands;
			}
			else
			{
				pool->first = commands;
			}
			pool->last = commands;
		}
		const VkCommandBufferBeginInfo beginInfo{
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
			.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
		};
		VkResult commandBeginResult = vkBeginCommandBuffer(commands->commandBuffer, &beginInfo);
		if (commandBeginResult != VK_SUCCESS)
		{
			std::cout << "Command recording begin failed. Error code: " << commandBeginResult << std::endl;
		}

		commands->timestampCount = 0;
		commands->submitted = false;
		commands->has_epilogue = false;

		if (commands->timestampPool)
			vkResetQueryPool(pool->state->device, commands->timestampPool, 0, pool->state->timestampQueryCount);
		return commands;
	}

	void endCommands(CommandBuffer* commands)
	{
		VkCommandBuffer commandBuffer = commands->commandBuffer;
		if (commands->swapchain)
		{
			VkResult endCommandResult = vkEndCommandBuffer(commandBuffer);
			if (endCommandResult != VK_SUCCESS)
			{
				std::cout << "Failed to end command buffer recording. Error Code: " << endCommandResult << std::endl;
			}
			if (!commands->epilogue)
			{
				const VkCommandBufferAllocateInfo allocateInfo
				{
					.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
					.commandPool = commands->commandPool,
					.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
					.commandBufferCount = 1,
				};
				VkResult allocateResult = vkAllocateCommandBuffers(commands->state->device, &allocateInfo, &commands->epilogue);
				if (allocateResult != VK_SUCCESS)
				{
					throw std::runtime_error("FAILED TO ALLOCATE COMMAND BUFFER LINE 745");
				}
			}
			const VkCommandBufferBeginInfo beginInfo
			{
				.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
				.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
			};
			commandBuffer = commands->epilogue;
			VkResult beginRecordingResult = vkBeginCommandBuffer(commandBuffer, &beginInfo);
			commands->has_epilogue = true;
			Swapchain* swapchain = commands->swapchain;
			const VkImageMemoryBarrier2 barrier{
				.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
				.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
				.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT,
				.dstStageMask = VK_PIPELINE_STAGE_2_NONE,
				.oldLayout = VK_IMAGE_LAYOUT_GENERAL,
				.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
				.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
				.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
				.image = swapchain->images[swapchain->imageIndex],
				.subresourceRange = {
					.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
					.levelCount = 1,
					.layerCount = 1,
				},
			};
			std::vector<VkImageMemoryBarrier2> barriers;
			barriers.push_back(barrier);
			recordImageBarriers(commandBuffer, barriers);
		}
		VkResult endCommandRecordingResult = vkEndCommandBuffer(commandBuffer);
		if (endCommandRecordingResult != VK_SUCCESS)
		{
			std::cout << "Failed to end command recording. Error code: " << endCommandRecordingResult << std::endl;
		}
	}

	void recordImageBarriers(VkCommandBuffer commandBuffer, const std::vector<VkImageMemoryBarrier2> barriers)
	{
		const VkDependencyInfo dependency{
			.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
			.imageMemoryBarrierCount = barriers.size(),
			.pImageMemoryBarriers = barriers.data(),
		};
		vkCmdPipelineBarrier2(commandBuffer, &dependency);
	}

	void gpuBeginRenderPass(CommandBuffer* buffer)
	{
		VkRenderingAttachmentInfo colorAttatchments[maxColorAttachments];
		for (size_t index = 0; index < maxColorAttachments; ++index)
		{
			colorAttatchments[index] = {
				.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
				.imageView = attachment.render_view->view,
				.imageLayout = VK_IMAGE_LAYOUT_GENERAL,
				.loadOp = static_cast<VkAttachmentLoadOp>(attachment.load),
				.storeOp = static_cast<VkAttachmentStoreOp>(attachment.store),
				.clearValue = {
					.color = {
						.float32 = { attachment.clear.x, attachment.clear.y, attachment.clear.z, attachment.clear.w },
					},
				},
			};
		}
		const VkRenderingInfo renderingInfo{
			.sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
			.flags = static_cast<VkRenderingFlags>(flags),
			.renderArea = {
				.extent = { .width = area_view->width, .height = area_view->height },
			},
			.layerCount = 1,
			.colorAttachmentCount = static_cast<uint32>(desc.colors.size),
			.pColorAttachments = color_attachments,
			.pDepthAttachment = desc.depth.render_view ? &depth_attachment : nullptr,
			.pStencilAttachment = desc.stencil.render_view ? &stencil_attachment : nullptr,
		};

		

		// Render the info sent to command buffer
		vkCmdBeginRendering(buffer->commandBuffer, &renderingInfo);

	}

	void gpuEndRenderPass(CommandBuffer* buffer)
	{
		vkCmdEndRendering(buffer->commandBuffer);
	}

	void submitCommands(Device* device, const SubmitDesc& desc, uint32_t queue_index)
	{

	}

	void submitAndPresent(Device* device, const SubmitDesc& desc)
	{
		Swapchain* currentChain = device->swapChain;

		PresentContext& context = *currentChain->presentContext;
	}

	// GPU Queue methods
	void gpuCreateQueue()
	{

	}
	void gpuStartCommandRecording(Queue& queue)
	{

	}
	void gpuSubmitCommandBuffers()
	{

	}

	// Deallocate any allocated resources
	void cleanup()
	{

	}


} // gpu


namespace gpu::present
{
	struct ClearColor
	{
		float r = 0.0f;
		float g = 0.0f;
		float b = 0.0f;
		float a = 0.0f;
	};

	struct RenderView
	{

	};


	struct Stencil
	{

	};

	struct DepthStencilDesc
	{

	};

	struct BlendDesc 
	{

	};

	struct RasterizerDesc
	{

	};

	struct RenderPassDesc
	{

	};

	struct RenderingDesc
	{

	};

	struct TextureDesc
	{

	};

	struct SubmitDesc
	{
		std::vector<CommandBuffer* const> commands;
		std::vector<const sync::TimelinePoint> waits;
		sync::TimelinePoint completion = {};
	};


	// Render View methods
	RenderView* createRenderView();
	void destroyRenderView();

	// Stencil, color blending, and other description methods here
	void createBlendState();
	void modifyBlendState();
	void createRenderPassState();
	void modifyRenderPassState();
	void createDepthStencilState();
	void modifyDepthStencilState();
	void submitAndPresent(Device* device, SubmitDesc& desc)
	{
		Swapchain* swapchain = device->swapChain;
		PresentContext& context = *swapchain->presentContext;

		VkResult resetFenceResult = vkResetFences(device->device, 1, &context.presented);
	}
} // present


namespace gpu::pipeline
{
	// My thought process on this(custom and flexible render pipelines):
	/*
	*	CustomPipeline will serve as the starting point of a linked list of however many shader stages a user wants
	*	CustomStage will hold the stage and the pointer to next stage
	*	a generateCustomPipeline() method will automaically create the needed PSOs for this new pipeline, to which bindPSO can then be used
	*	Issues with this idea:
	*		1. In terms of passing in info easily for models and such I am unsure how that will work
	*		2. Is probably very cumbersome to maintain and keep track of programming wise
	*		3. The possibly many different implementation problems
	*		4. Oh god the synchronization
	*	Assumptions that the custom pipeline will (probably) make
	*		1. The final stage will always present whatever was just rendered
	*		2. 1st stage will (probably) take vertex information, even if it does nothing with it
	*	For now these will remain unused, because as of writing, the main goal is to get a triangle to render, not create 
	*	the ability for an entire custom render pipeline
	*	
	*	Note in issue 1: command buffers probably handle this issue to some extent, research on the Vulkan command pools/buffers definitely needed
	*	
	*	As an extra little theory, in multiple or 1 custom pipeline, you can have 1 stage point to 2 separate stages, and potentially merge back into the pipeline at some point
	*	This would allow for any non-sense combination of pipeline stages with however many different render settings/combinations a user could want
	*	How to implement this, not sure.
	*/
	struct CustomPipeline
	{
		ShaderStage* firstStage = nullptr;
		uint32_t numStages = 0;

		bool addStage();
	};

	struct CustomStage
	{
		ShaderStage stage = {};
		ShaderStage* nextStage = nullptr;
		ShaderStage* sideStage = nullptr;
	};

	struct ShaderStage
	{
		const char* stageName = nullptr;
		const char* entryPoint = nullptr;
		// NOTE: noapi states threadgroups variable is for Metal

		std::vector<char> code;
	};

	struct GraphicsPSODesc
	{
		ShaderStage vertex = {};
		ShaderStage fragment = {};
	};

	// bar-for-bar from no-api github
	// TODO: will probably need to be changed in the future
	struct PSO
	{
		Device* currentState = nullptr;
		VkPipeline pso = VK_NULL_HANDLE;

		VkPipelineBindPoint bindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

		~PSO()
		{
			if (currentState && pso) vkDestroyPipeline(currentState->device, pso, nullptr);
		}
	};

	struct RasterState
	{

	};

	std::vector<char*> readShader(std::string fileName, std::string filePath)
	{
		std::vector<char*> results;

		return results;
	}

	void bindPSO(CommandBuffer* commands, const PSO* pso)
	{
		vkCmdBindPipeline(commands->commandBuffer, pso->bindPoint, pso->pso);
	}

	// TODO: currently hardcoded depth, stencil, and color target descriptors, replace later with flexible values
	// NOTE: Randomly picked values, if triangle is wrong problem is likely here
	PSO* createRasterPSO(Device* device, const ShaderStage& vertStage, const ShaderStage& fragStage, const RasterizationState& rastState, const ShaderStage& task = {}, bool mesh)
	{
		const VkShaderModuleCreateInfo taskInfo
		{
			.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
			.codeSize = task.code.size(),
			.pCode = reinterpret_cast<const uint32_t*>(task.code.data()),
		};
		const VkShaderModuleCreateInfo vertexInfo
		{
			.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
			.codeSize = vertStage.code.size(),
			.pCode = reinterpret_cast<const uint32_t*>(vertStage.code.data()),
		};
		const VkShaderModuleCreateInfo fragInfo
		{
			.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
			.codeSize = fragStage.code.size(),
			.pCode = reinterpret_cast<const uint32_t*>(fragStage.code.data()),
		};

		const VkShaderDescriptorSetAndBindingMappingInfoEXT mappingInfo[]
		{
			{ .sType = VK_STRUCTURE_TYPE_SHADER_DESCRIPTOR_SET_AND_BINDING_MAPPING_INFO_EXT, .pNext = &taskInfo,
			 .mappingCount = 1, .pMappings = &rootMapping },
			{ .sType = VK_STRUCTURE_TYPE_SHADER_DESCRIPTOR_SET_AND_BINDING_MAPPING_INFO_EXT, .pNext = &vertexInfo,
			 .mappingCount = 1, .pMappings = &rootMapping },
			{ .sType = VK_STRUCTURE_TYPE_SHADER_DESCRIPTOR_SET_AND_BINDING_MAPPING_INFO_EXT, .pNext = &fragInfo,
			 .mappingCount = 1, .pMappings = &rootMapping },
		};
		const VkPipelineShaderStageCreateInfo stages[]{
			VkPipelineShaderStageCreateInfo{
				.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
				.pNext = &mappingInfo[0],
				.stage = VK_SHADER_STAGE_TASK_BIT_EXT,
				.pName = task.entryPoint,
			},
			VkPipelineShaderStageCreateInfo{
				.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
				.pNext = &mappingInfo[1],
				.stage = mesh ? VK_SHADER_STAGE_MESH_BIT_EXT : VK_SHADER_STAGE_VERTEX_BIT,
				.pName = vertStage.entryPoint,
			},
			VkPipelineShaderStageCreateInfo{
				.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
				.pNext = &mappingInfo[2],
				.stage = VK_SHADER_STAGE_FRAGMENT_BIT,
				.pName = fragStage.entryPoint,
			},
		};

		const VkPipelineVertexInputStateCreateInfo vertexInput{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
		};
		const VkPipelineInputAssemblyStateCreateInfo inputAssembly{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
			.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
		};
		const VkPipelineViewportStateCreateInfo viewportState{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
		};

		// TODO: this is a lot of hardcoded values made to simplify getting the triangle on screen, change later to user-defined valuies
		const VkPipelineRasterizationStateCreateInfo rasterization{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
			.polygonMode = VK_POLYGON_MODE_FILL,
			.cullMode = VK_CULL_MODE_NONE,
			.frontFace = VK_FRONT_FACE_CLOCKWISE,
			.depthBiasEnable = VK_FALSE,
			.depthBiasConstantFactor = 0.0f,
			.depthBiasClamp = 0.0f,
			.depthBiasSlopeFactor = 0.0f,
			.lineWidth = 1.0f,
		};
		const VkPipelineMultisampleStateCreateInfo multisample{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
			.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
		};
		const VkPipelineDepthStencilStateCreateInfo depthStencil{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
		};
		VkPipelineColorBlendAttachmentState colorAttachments[maxColorAttachments]{};
		for (size_t index = 0; index < maxColorAttachments; ++index) // TODO: massively simplified from noapi, get that information laterr
		{
			colorAttachments[index] = {
				.blendEnable = VK_FALSE,
				.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
			};
		}

		const VkPipelineColorBlendStateCreateInfo colorBlend{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
			.attachmentCount = maxColorAttachments,
			.pAttachments = colorAttachments,
		};
		constexpr VkDynamicState dynamicStates[]{
			VK_DYNAMIC_STATE_VIEWPORT_WITH_COUNT,
			VK_DYNAMIC_STATE_SCISSOR_WITH_COUNT,
			VK_DYNAMIC_STATE_DEPTH_TEST_ENABLE,
			VK_DYNAMIC_STATE_DEPTH_WRITE_ENABLE,
			VK_DYNAMIC_STATE_DEPTH_COMPARE_OP,
			VK_DYNAMIC_STATE_STENCIL_TEST_ENABLE,
			VK_DYNAMIC_STATE_STENCIL_OP,
			VK_DYNAMIC_STATE_STENCIL_COMPARE_MASK,
			VK_DYNAMIC_STATE_STENCIL_WRITE_MASK,
			VK_DYNAMIC_STATE_STENCIL_REFERENCE,
		};
		const VkPipelineDynamicStateCreateInfo dynamicState{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
			.dynamicStateCount = static_cast<uint32_t>(sizeof(dynamicStates) / sizeof(dynamicStates[0])),
			.pDynamicStates = dynamicStates,
		};

		VkFormat colorFormats[maxColorAttachments]{};
		for (size_t index = 0; index < maxColorAttachments; ++index)
		{
			colorFormats[index] = VK_FORMAT_B8G8R8_SRGB;
		}
		const VkPipelineRenderingCreateInfo renderingInfo{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
			.colorAttachmentCount = maxColorAttachments,
			.pColorAttachmentFormats = colorFormats,
			.depthAttachmentFormat = VK_FORMAT_UNDEFINED,
			.stencilAttachmentFormat = VK_FORMAT_UNDEFINED,
		};
		const VkPipelineCreateFlags2CreateInfo flagsInfo{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_CREATE_FLAGS_2_CREATE_INFO,
			.pNext = &renderingInfo,
			.flags = VK_PIPELINE_CREATE_2_DESCRIPTOR_HEAP_BIT_EXT,
		};
		const VkGraphicsPipelineCreateInfo psoInfo{
			.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
			.pNext = &flagsInfo,
			.stageCount = 1u + (task.code.size() ? 1u : 0u) + (fragStage.code.size() ? 1u : 0u),
			.pStages = stages + (task.code.size() ? 0u : 1u),
			.pVertexInputState = mesh ? nullptr : &vertexInput,
			.pInputAssemblyState = mesh ? nullptr : &inputAssembly,
			.pViewportState = &viewportState,
			.pRasterizationState = &rasterization,
			.pMultisampleState = &multisample,
			.pDepthStencilState = &depthStencil,
			.pColorBlendState = &colorBlend,
			.pDynamicState = &dynamicState,
			.basePipelineIndex = -1,
		};
		PSO* result = new PSO{
			.currentState = device,
			.bindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
		};

		return result;
	}

	PSO* createGraphicsPSO(Device* device, GraphicsPSODesc graphicsDesc)
	{

	}

} // pipeline

namespace gpu::shader
{

}  // shader


namespace gpu::sync
{
	struct TimelineSemaphore
	{
		Device* state = nullptr;
		VkSemaphore semaphore = VK_NULL_HANDLE;
	};
	struct TimelinePoint
	{
		TimelineSemaphore* semaphore = nullptr;
		uint64_t value = 0;
	};
} // sync