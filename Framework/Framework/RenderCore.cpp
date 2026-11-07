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
	const uint32_t queueTypeCount = 3; // noapi only uses 3 queues: Compute, Graphics, Transfer. Later on we will probbaly need more than this

	

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
		VkInstance vkInstance = VK_NULL_HANDLE;
		VkSurfaceKHR surface = VK_NULL_HANDLE;


		uint32_t queueCount = 0;
		uint32_t queueFamilyCount = 0;
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
		const char* gpuName;
		// Queue Capabilities
		uint32_t queueCount = 0;
		uint32_t queueGeneralCount = 0;
		uint32_t queueComputeCount = 0;
		uint32_t queueTransferCount = 0; //  Also called copy queue in no_api
		// TODO: add compression texture stuff here if we go that route
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
	uint32_t scoreDevice(VkPhysicalDevice device)
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
		std::vector<VkExtensionProperties> extensionProperties;
		std::vector<VkQueueFamilyProperties2> queueFamilyProperties;
		VkPhysicalDeviceProperties2 deviceProperties2;
		VkPhysicalDeviceFeatures2   deviceCoreFeatures2{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2 };
		VkPhysicalDeviceVulkan11Features   deviceVk11Features{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES };
		VkPhysicalDeviceVulkan12Features   deviceVk12Features{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES };
		VkPhysicalDeviceVulkan13Features   deviceVk13Features{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES };
		VkPhysicalDeviceVulkan14Features   deviceVk14Features{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_4_FEATURES };
		VkPhysicalDeviceDescriptorHeapFeaturesEXT deviceHeapFeatures{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_HEAP_FEATURES_EXT };
		VkPhysicalDeviceMemoryProperties2 deviceMemProperties{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2 };
		// NOTE: Despite querying for its support, the variables relating to the extension do not exist
		// In noapi they dont list it as an enabled extension either
		// So I am unsure why this is here other than for checking that the current GPU can use this
		//VkPhysicalDeviceDeviceAddressCommandsFeaturesKHR address_commands{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEVICE_ADDRESS_COMMANDS_FEATURES_KHR };
		
		

		// Struct chaining
		deviceCoreFeatures2.pNext = &deviceVk11Features;
		deviceVk11Features.pNext = &deviceVk12Features;
		deviceVk12Features.pNext = &deviceVk13Features;
		deviceVk13Features.pNext = &deviceVk14Features;
		deviceVk14Features.pNext = &deviceHeapFeatures;
		
		// Get extensions
		VkResult extensionResult = vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, extensionProperties.data());
		if (extensionResult == VK_INCOMPLETE)
		{
			return 0;
		}

		// Get device properties and iterate to find all required properties
		vkGetPhysicalDeviceProperties2(device, &deviceProperties2);
		vkGetPhysicalDeviceFeatures2(device, &deviceCoreFeatures2);
		vkGetPhysicalDeviceMemoryProperties2(device, &deviceMemProperties);
		

		// If any of these fail, device is not suitable and will score 0
		const bool requiredFeatures =
			deviceCoreFeatures2.features.shaderInt16 == VK_TRUE &&
			deviceCoreFeatures2.features.samplerAnisotropy == VK_TRUE &&
			deviceCoreFeatures2.features.depthBiasClamp == VK_TRUE &&
			deviceCoreFeatures2.features.independentBlend == VK_TRUE &&
			deviceCoreFeatures2.features.fragmentStoresAndAtomics == VK_TRUE &&
			deviceCoreFeatures2.features.vertexPipelineStoresAndAtomics == VK_TRUE &&
			deviceCoreFeatures2.features.shaderStorageImageReadWithoutFormat == VK_TRUE &&
			deviceCoreFeatures2.features.shaderStorageImageWriteWithoutFormat == VK_TRUE &&
			deviceCoreFeatures2.features.multiDrawIndirect == VK_TRUE &&
			deviceCoreFeatures2.features.drawIndirectFirstInstance == VK_TRUE &&
			deviceVk11Features.storageBuffer16BitAccess == VK_TRUE &&
			deviceVk11Features.storagePushConstant16 == VK_TRUE &&
			deviceVk11Features.shaderDrawParameters == VK_TRUE &&
			deviceVk12Features.shaderFloat16 == VK_TRUE &&
			deviceVk12Features.scalarBlockLayout == VK_TRUE &&
			deviceVk12Features.bufferDeviceAddress == VK_TRUE &&
			deviceVk12Features.timelineSemaphore == VK_TRUE &&
			deviceVk13Features.synchronization2 == VK_TRUE &&
			deviceVk13Features.dynamicRendering == VK_TRUE &&
			deviceVk13Features.maintenance4 == VK_TRUE &&
			deviceVk14Features.maintenance5 == VK_TRUE; // TODO: addressCommands seems to be missing from my Vulkan, fix later

		if (!requiredFeatures)
		{
			return 0;
		}

		// Determine queue count and families, check that needed queues exist
		vkGetPhysicalDeviceQueueFamilyProperties2(device, &queueCount, queueFamilyProperties.data());
		for (uint32_t index = 0; index < queueCount; ++index)
		{
			if (queueFamilyProperties.at(index).queueFamilyProperties.queueCount == 0)
				continue;

			const VkFlags queueFlag = queueFamilyProperties.at(index).queueFamilyProperties.queueFlags;

		}

		return score;

	}

	// Find physical device
	// Refer to inspect_candidate in no_api
	bool pickPhysicalDevice(VkInstance instance)
	{
		uint32_t deviceCount = 0;
		std::vector<VkPhysicalDevice> devices;
		uint32_t currentBestScore = 0;
		VkPhysicalDevice bestDevice;
		//std::vector<VkPhysicalDevice> deviceList;
		VkResult physicalDeviceResult = vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

		if (physicalDeviceResult == VK_INCOMPLETE)
		{
			return false;
		}
		
		// Iterate each device and run check
		for (const auto& device : devices)
		{
			uint32_t currentScore = scoreDevice(device);
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
		
	}

	// TODO: add if (DEBUG) condition for the validation layer
	bool createDevice(Device* device)
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

		// Find physical device to use and begin populating the rest of the  Device* device param

		// Queue selection


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