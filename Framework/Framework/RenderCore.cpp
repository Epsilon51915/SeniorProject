#include "RenderCore.h"


namespace gpu {


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
	const uint32_t queueTypeCount = 3; // The 3 queues: Compute, Graphics, Transfer.

	

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
		std::vector<uint32_t> queueFamilies;
		uint32_t textureMemoryType = VK_MAX_MEMORY_TYPES;
		uint64_t textureHeapAlignment = 16;

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
		const char* gpuName = nullptr;
		// Queue Capabilities
		uint32_t queueCount = 0;
		uint32_t queueGeneralCount = 0;
		uint32_t queueComputeCount = 0;
		uint32_t queueTransferCount = 0; //  Also called copy queue in no_api
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
		VkColorSpaceKHR colorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
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
		VkQueryPool timestamp_pool = VK_NULL_HANDLE;

		Swapchain* swapchain = nullptr;
		bool submitted = false;
		bool has_epilogue = false;
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

	// bar-for-bar from no-api github
	// TODO: will probably need to be changed in the future
	struct PSO
	{
		Device* currentState = nullptr;
		VkPipeline pso = VK_NULL_HANDLE;

		VkPipelineBindPoint bind_point = VK_PIPELINE_BIND_POINT_GRAPHICS;

		~PSO()
		{
			if (currentState && pso) vkDestroyPipeline(currentState->device, pso, nullptr);
		}
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
		if (extensionResult == VK_INCOMPLETE)
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


		if (physicalDeviceResult == VK_INCOMPLETE)
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
		uint32_t extensionCount = 0;
		std::vector<VkExtensionProperties> extensions;
		std::vector<const char*> enabledExtensions;
		std::vector<const char*> enabledLayers;
		uint32_t enabledExtensionCount = 0;
		uint32_t layerCount = 1;
		VkResult extensionResult = vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data());

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
		

		// TODO: enable extensions here

		const VkInstanceCreateInfo instanceInfo
		{
			.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
			.pApplicationInfo = &appInfo,
			.enabledLayerCount = layerCount,
			.ppEnabledLayerNames = enabledLayers.data(),
			.enabledExtensionCount = enabledExtensionCount,
			.ppEnabledExtensionNames = enabledExtensions.data()
		};
		VkResult instanceResult = vkCreateInstance(&instanceInfo, nullptr, &device->vkInstance);

		if (instanceResult == VK_INCOMPLETE)
		{
			throw std::runtime_error("Failed to create VK Instance!");
		}

		// Find physical device to use and begin populating the rest of the Device* device param
		// TODO: Get physicalDevice Heap properties
		pickPhysicalDevice(device->vkInstance, device);
		vkGetPhysicalDeviceProperties2(device->physicalDevice, &device->deviceProps);
		vkGetPhysicalDeviceMemoryProperties2(device->physicalDevice, &device->deviceMemProps);
		

		DesiredFeatures enabledFeatures(true, true, true); // TODO: remove magic boolean

		// Queue selection
		uint32_t queueCount = 0;
		std::vector<VkQueueFamilyProperties2> queueFamilyProperties;
		vkGetPhysicalDeviceQueueFamilyProperties2(device->physicalDevice, &queueCount, queueFamilyProperties.data());
		// For each found queue family, iterate and get desired queue amount, and assign it to device queue struct
		const uint32_t requestedQueueCounts[]{ desiredDevice.desiredGraphicsQueues, desiredDevice.desiredComputeQueues, desiredDevice.desiredTransferQueues };
		uint32_t queueCounts[queueTypeCount];
		for (uint32_t queueType = 0; queueType < queueCount; ++queueType)
		{
			// TODO: Possibly correct logic, debug for sure
			// NOTE: assign queue counts from requestedQueues[] if lower than the queues in the family we are looking at, otherwise assign queue counts from family if requestedQueues[] is larger
			queueCounts[queueType] = requestedQueueCounts[queueType] < queueFamilyProperties.at(queueType).queueFamilyProperties.queueCount ? requestedQueueCounts[queueType] : queueFamilyProperties.at(queueType).queueFamilyProperties.queueCount;
			device->queueCount += queueCounts[queueType];
		}


		float* queuePriorities = new float[device->queueCount];
		// Assign queue priorities
		for (uint32_t index = 0; index < device->queueCount; ++index)
		{
			queuePriorities[index] = 1.0f;
		}

		VkDeviceQueueCreateInfo queueInfos[queueTypeCount]{};
		uint32_t firstQueue = 0;
		for (uint32_t queueType = 0; queueType < queueTypeCount; ++queueType)
		{
			if (queueCounts[queueType] == 0)
			{
				std::cout << "Queue family of index: " << queueType << ", doesn't have any queues assigned, ensure this isn't our graphics/present queue!" << std::endl;
				continue;
			}
			device->queueFamilies[device->queueFamilyCount]
		}


		const VkDeviceCreateInfo deviceInfo
		{
			.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
		};




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


	void gpuBeginRenderPass(CommandBuffer* buffer)
	{

		VkRenderingInfo renderInfo;

		VkRenderingAttachmentInfo colorAttatchments[maxColorAttachments];

		// Render the info sent to command buffer
		vkCmdBeginRendering(buffer->commandBuffer, &renderInfo);

	}

	void bindPSO()
	{

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
} // present


namespace gpu::pipeline
{

} // pipeline

namespace gpu::shader
{

}  // shader


namespace gpu::sync
{
	struct TimelinePoint;
} // sync