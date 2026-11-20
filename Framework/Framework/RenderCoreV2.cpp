#include "RenderCoreV2.hpp"


namespace core {

	// Important program variables
	constexpr uint32_t maxDeviceExtensions	 = 512;
	constexpr uint32_t maxInstanceExtensions = 512;
	constexpr uint32_t maxInstanceLayers	 = 64;
	constexpr uint32_t maxSurfaceFormats	 = 64;
	constexpr uint32_t maxQueueFamilies		 = 64; // So unbeleivably unlikely but sure
	constexpr uint32_t queueTypeCount		 = 3; // The 3 queues: Graphics, Compute, Transfer.
	constexpr uint32_t maxColorAttachments	 = 8;
	constexpr uint32_t maxSwapchainImages	 = 3;
	constexpr uint32_t formatCount = static_cast<uint32_t>(Format::undefined);
	constexpr VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;

	constexpr VkDescriptorSetAndBindingMappingEXT rootMapping{
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_AND_BINDING_MAPPING_EXT,
		.bindingCount = 1,
		.resourceMask = VK_SPIRV_RESOURCE_TYPE_UNIFORM_BUFFER_BIT_EXT | VK_SPIRV_RESOURCE_TYPE_READ_ONLY_STORAGE_BUFFER_BIT_EXT,
		.source = VK_DESCRIPTOR_MAPPING_SOURCE_PUSH_ADDRESS_EXT,
		.sourceData = { .pushAddressOffset = 0 },
	};


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

	template<typename T>
	T loadDeviceProc(VkDevice device, const char* name) noexcept
	{
		const PFN_vkVoidFunction proc = vkGetDeviceProcAddr(device, name);
		return reinterpret_cast<T>(proc);
	}

	struct RenderView
	{
		Device* state = nullptr;
		VkImageView view = VK_NULL_HANDLE;
		uint32_t width = 0;
		uint32_t height = 0;
		bool swapchainView = false;
	};

	struct RetiredSwapchain
	{
		VkSwapchainKHR handle = VK_NULL_HANDLE;
		VkImageView views[maxSwapchainImages]{};
		uint32_t viewCount = 0;
	};

	struct DeferredSwapchainImage
	{
		uint64_t retireValue = 0;
		VkSwapchainKHR swapchain = VK_NULL_HANDLE;
		VkImageView view = VK_NULL_HANDLE;
	};

	struct SwapchainDeleteQueue
	{

		DeferredSwapchainImage* entries = nullptr;
		size_t capacity = 0;
		size_t first = 0;
		size_t count = 0;

		void push(uint64_t retireValue, VkSwapchainKHR swapchain, VkImageView view) noexcept
		{
			//assert(swapchain && view);
			if (count == capacity)
			{
				const size_t old_size = capacity;
				capacity = old_size == 0 ? 1 : old_size * 2;
				entries = static_cast<DeferredSwapchainImage*>(realloc(entries, capacity * sizeof(DeferredSwapchainImage)));
				for (size_t index = 0; index < first; ++index)
				{
					entries[old_size + index] = entries[index];
					entries[index] = {};
				}
			}
			if (count != 0)
			{
				const size_t back = (first + count - 1) % capacity;
				//assert(entries[back].retireValue <= retire_value &&
				//	"swapchain deletion retire values must be monotonic");
			}
			entries[(first + count) % capacity] = {
				.retireValue = retireValue,
				.swapchain = swapchain,
				.view = view,
			};
			++count;
		}

		void collect(VkDevice device, uint64_t completedValue) noexcept
		{
			while (count != 0 && entries[first].retireValue <= completedValue)
			{
				const DeferredSwapchainImage& entry = entries[first];
				const bool final_image = count == 1 || entries[(first + 1) % capacity].swapchain != entry.swapchain;
				vkDestroyImageView(device, entry.view, nullptr);
				if (final_image)
					vkDestroySwapchainKHR(device, entry.swapchain, nullptr);
				entries[first] = {};
				first = (first + 1) % capacity;
				--count;
			}
			if (count == 0) first = 0;
		}

	};

	// Series of images of rendered images that will be presented
	struct Swapchain
	{
		Device* state = nullptr;
		VkSwapchainKHR handle = VK_NULL_HANDLE;

		VkImage images[maxSwapchainImages]{};
		RenderView renderViews[maxSwapchainImages]{};

		//present::RenderView renderViews[maxSwapchainImages]{};
		uint32_t imageIndex = 0;
		uint32_t imageCount = 0;
		uint32_t width = 0;
		uint32_t height = 0;
		VkColorSpaceKHR colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR; // Default image format that will be used
		Format format = Format::bgra8_srgb;
		VkSurfaceTransformFlagBitsKHR transform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
		VkCompositeAlphaFlagBitsKHR compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
		CommandBuffer* transitionCommands = nullptr;
		bool acquired = false;
		bool recreateRequired = false; //  Happens when resolution changes/minimized or maximized.
		bool initialized[maxSwapchainImages]{};

		PresentContext* presentContext = nullptr;

	};

	// Command Pool/Buffer definitions. Pools chain buffers using linked lists TODO: Potentially make into vector for easier use?
	struct CommandPool
	{
		Device* state = nullptr;
		VkCommandPool commandPool = VK_NULL_HANDLE;
		CommandBuffer* first = nullptr;
		CommandBuffer* last = nullptr;
		CommandBuffer* nextBuffer = nullptr;
		bool timestamps = false;
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
		bool hasEpilogue = false;

		uint32_t timestampCount = 0;
		uint64_t** timestampDestinations = nullptr;
		uint64_t* timestampResults = nullptr;
	};

	// The queue counts, features, and other aspects of the physical/logical device we are using
	// TODO: Consider if this struct is really needed as these variables can honestly be in Device and we have 1 less struct
	struct DeviceSpecs
	{
		const char* deviceName;
		

		// Queue Type Information
		uint32_t queueCount = 0;
		uint32_t graphicsQueueCount = 0;
		uint32_t computeQueueCount	= 0;
		uint32_t transferQueueCount = 0;

		uint32x3 copyTextureGranularity = { .x = 1, .y = 1, .z = 1 }; // Used by transfer queue

		uint64_t maxPushDataSize = 0;
		uint64_t textureHeapAlignment = 0;
		float timestampPeriodns = 0.0f; // Nanoseconds per timestamp tick; zero when profiling is unavailable.
		uint32_t subTexelPrecisionBits = 0; // Fractional filtering precision, for conservative sampled-field bounds.
		bool textureCompressionBc = false;
		bool textureCompressionAstc = false;
		bool storageInputOutput16 = false;
		bool indirectMeshDraw = false;
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
		VkPhysicalDeviceProperties2 deviceProperties{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2 };
		VkPhysicalDeviceMemoryProperties2 deviceMemProps{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2 };
		VkPhysicalDeviceDescriptorHeapPropertiesEXT deviceHeapProps{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_HEAP_PROPERTIES_EXT };

		// Queue Family Information
		uint32_t totalQueueCount = 0;
		uint32_t queueFamilyCount = 0;
		uint32_t queueFamiliesIndex[queueTypeCount]{};

		// Command buffer information
		uint32_t timestampQueryCount = 0;

		// Texture Information
		uint64_t textureHeapAlignment	= 16; // 16 what? Define this.
		uint32_t textureMemoryType		= VK_MAX_MEMORY_TYPES;
		bool textureCompressionEtc2		= false;
		bool swapchainColorSpaceEnabled = false;

		// Swapchain/Present Information
		SwapchainDeleteQueue swapchainDeleteQueue;
		RetiredSwapchain retiredSwapchains[maxSwapchainImages]{};
		VkFormatFeatureFlags2 formatFeatures[formatCount]{};

		// Present Information
		PresentContext presentContexts[maxSwapchainImages]{};
		VkSemaphore presentationRetirement		 = VK_NULL_HANDLE;
		uint32_t presentContextCount			 = 0;
		uint32_t nextPresentContext				 = 0;
		uint64_t presentationRetirementValue	 = 0;
		uint64_t completedPresentationRetirement = 0;
		uint64_t maxTimelineValueDifference		 = 0;

		// Device methods
		void pollPresentationRetirement();
		VkResult createPresentContext(PresentContext& context);
		void destroyPresentContext(PresentContext& context) noexcept;
		void pollPresentContexts() noexcept;
		void finishPresentContext(PresentContext& context) noexcept;
		void waitPresentContext(PresentContext& context) noexcept;
		void queueRetiredSwapchain(RetiredSwapchain& retired) noexcept;
		void drainContexts() noexcept;
		void waitPresentationRetirement(uint64_t value);
		uint64_t nextPresentationRetirement() noexcept;

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

		std::vector<const char*> enabledExtensions;
	};

	struct Queue
	{
		VkQueue queue = VK_NULL_HANDLE;
		bool timestamps = false;
		VkCommandBufferSubmitInfo* commandSubmitInfos = nullptr;
		VkSemaphoreSubmitInfo* waitSubmitInfos = nullptr;
		uint32_t familyIndex = 0;
		size_t commandSubmitCapacity = 0;
		size_t waitSubmitCapacity = 0;
	};

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

	// Device method defintions
	Device::~Device()
	{
		if (device)
		{
			drainContexts();
			destroyOwnedSwapchain(*this);
			swapchainDeleteQueue.collect(device, completedPresentationRetirement);
			//assert(swapchain_delete_queue.count == 0);
		}
		if (device && presentationRetirement) vkDestroySemaphore(device, presentationRetirement, nullptr);
		for (uint32_t index = 0; index < totalQueueCount; ++index)
		{
			free(queueArray[index].commandSubmitInfos);
			free(queueArray[index].waitSubmitInfos);
		}
		delete[] queueArray;
		//delete buffer_map;
		free(swapchainDeleteQueue.entries);
		for (uint32_t index = 0; index < presentContextCount; ++index)
		{
			destroyPresentContext(presentContexts[index]);
		}
		if (device) vkDestroyDevice(device, nullptr);
		if (instance && surface) vkDestroySurfaceKHR(instance, surface, nullptr);
		//if (instance && debug_messenger && destroyDebugMessenger) destroy_debug_messenger(instance, debug_messenger, nullptr);
		if (instance) vkDestroyInstance(instance, nullptr);
	}

	void Device::pollPresentationRetirement()
	{
		if (!presentationRetirement || completedPresentationRetirement == presentationRetirementValue)
			return;
		uint64_t completed = 0;
		vkGetSemaphoreCounterValue(device, presentationRetirement, &completed);
		//assert(completed >= completed_presentation_retirement && completed <= presentation_retirement_value);
		if (completed == completedPresentationRetirement)
			return;
		completedPresentationRetirement = completed;
		swapchainDeleteQueue.collect(device, completedPresentationRetirement);
	}

	void Device::waitPresentationRetirement(uint64_t value)
	{
		//assert(value <= presentation_retirement_value);
		if (value <= completedPresentationRetirement)
		{
			swapchainDeleteQueue.collect(device, completedPresentationRetirement);
			return;
		}
		const VkSemaphoreWaitInfo wait_info{
			.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO,
			.semaphoreCount = 1,
			.pSemaphores = &presentationRetirement,
			.pValues = &value,
		};
		vkWaitSemaphores(device, &wait_info, ~uint64_t{ 0 });
		completedPresentationRetirement = value;
		swapchainDeleteQueue.collect(device, completedPresentationRetirement);
	}

	void Device::drainContexts() noexcept
	{
		waitPresentationRetirement(presentationRetirementValue);
		for (uint32_t index = 0; index < presentContextCount; ++index)
		{
			waitPresentContext(presentContexts[index]);
		}
		for (const RetiredSwapchain& retired : retiredSwapchains)
		{
			//assert(!retired.handle);
		}
		swapchainDeleteQueue.collect(device, completedPresentationRetirement);
	}

	void Device::pollPresentContexts() noexcept
	{
		for (uint32_t index = 0; index < presentContextCount; ++index)
		{
			PresentContext& context = presentContexts[index];
			if (!context.presentPending)
				continue;
			const VkResult result = vkGetFenceStatus(device, context.presented);
			if (result == VK_NOT_READY)
				continue;
			//assert_vk(result);
			context.presentPending = false;
			finishPresentContext(context);
		}
	}

	void Device::finishPresentContext(PresentContext& context) noexcept
	{
		//assert(!context.present_pending && context.swapchain);
		const VkSwapchainKHR completedSwapchain = context.swapchain;
		context.swapchain = VK_NULL_HANDLE;
		for (RetiredSwapchain& retired : retiredSwapchains)
		{
			if (retired.handle != completedSwapchain)
				continue;
			for (const PresentContext& pending : presentContexts)
			{
				if (pending.presentPending && pending.swapchain == completedSwapchain)
					return;
			}
			queueRetiredSwapchain(retired);
			return;
		}
	}

	void Device::destroyPresentContext(PresentContext& context) noexcept
	{
		//assert(!context.present_pending);
		if (device && context.acquired) vkDestroySemaphore(device, context.acquired, nullptr);
		if (device && context.rendered) vkDestroySemaphore(device, context.rendered, nullptr);
		if (device && context.presented) vkDestroyFence(device, context.presented, nullptr);
		context = {};
	}

	void Device::waitPresentContext(PresentContext& context) noexcept
	{
		if (!context.presentPending)
			return;
		vkWaitForFences(device, 1, &context.presented, VK_TRUE, ~uint64_t{ 0 });
		context.presentPending = false;
		finishPresentContext(context);
	}

	void Device::queueRetiredSwapchain(RetiredSwapchain& retired) noexcept
	{
		//assert(retired.handle && retired.viewCount != 0);
		for (uint32_t index = 0; index < retired.viewCount; ++index)
		{
			const VkImageView view = retired.views[index];
			//assert(view);
			swapchainDeleteQueue.push(presentationRetirementValue, retired.handle, view);
		}
		retired = {};
		swapchainDeleteQueue.collect(device, completedPresentationRetirement);
	}

	uint64_t Device::nextPresentationRetirement() noexcept
	{
		const uint64_t next = presentationRetirementValue + 1;
		if (next - completedPresentationRetirement > maxTimelineValueDifference)
		{
			pollPresentationRetirement();
			if (next - completedPresentationRetirement > maxTimelineValueDifference)
				waitPresentationRetirement(next - maxTimelineValueDifference);
			//assert(next - completedPresentationRetirement <= maxTimelineValueDifference);
		}
		presentationRetirementValue = next;
		return next;
	}

	VkFormat toVk(Format format)
	{
		switch (format)
		{
		case Format::r8_srgb: return VK_FORMAT_R8_SRGB;
		case Format::rg8_srgb: return VK_FORMAT_R8G8_SRGB;
		case Format::rgba8_srgb: return VK_FORMAT_R8G8B8A8_SRGB;
		case Format::bgra8_srgb: return VK_FORMAT_B8G8R8A8_SRGB;
		case Format::rgba4_unorm: return VK_FORMAT_R4G4B4A4_UNORM_PACK16;
		case Format::r5g5b5a1_unorm: return VK_FORMAT_R5G5B5A1_UNORM_PACK16;
		case Format::r5g6b5_unorm: return VK_FORMAT_R5G6B5_UNORM_PACK16;
		case Format::r8_unorm: return VK_FORMAT_R8_UNORM;
		case Format::rg8_unorm: return VK_FORMAT_R8G8_UNORM;
		case Format::rgba8_unorm: return VK_FORMAT_R8G8B8A8_UNORM;
		case Format::bgra8_unorm: return VK_FORMAT_B8G8R8A8_UNORM;
		case Format::r16_unorm: return VK_FORMAT_R16_UNORM;
		case Format::rg16_unorm: return VK_FORMAT_R16G16_UNORM;
		case Format::rgba16_unorm: return VK_FORMAT_R16G16B16A16_UNORM;
		case Format::r8_uint: return VK_FORMAT_R8_UINT;
		case Format::rg8_uint: return VK_FORMAT_R8G8_UINT;
		case Format::rgba8_uint: return VK_FORMAT_R8G8B8A8_UINT;
		case Format::bgra8_uint: return VK_FORMAT_B8G8R8A8_UINT;
		case Format::r16_uint: return VK_FORMAT_R16_UINT;
		case Format::rg16_uint: return VK_FORMAT_R16G16_UINT;
		case Format::rgba16_uint: return VK_FORMAT_R16G16B16A16_UINT;
		case Format::r32_uint: return VK_FORMAT_R32_UINT;
		case Format::rg32_uint: return VK_FORMAT_R32G32_UINT;
		case Format::rgba32_uint: return VK_FORMAT_R32G32B32A32_UINT;
		case Format::r16_float: return VK_FORMAT_R16_SFLOAT;
		case Format::rg16_float: return VK_FORMAT_R16G16_SFLOAT;
		case Format::rgba16_float: return VK_FORMAT_R16G16B16A16_SFLOAT;
		case Format::r32_float: return VK_FORMAT_R32_SFLOAT;
		case Format::rg32_float: return VK_FORMAT_R32G32_SFLOAT;
		case Format::rgba32_float: return VK_FORMAT_R32G32B32A32_SFLOAT;
		case Format::rgb10a2_unorm: return VK_FORMAT_A2B10G10R10_UNORM_PACK32;
		case Format::rg11b10_float: return VK_FORMAT_B10G11R11_UFLOAT_PACK32;
		case Format::d16_unorm: return VK_FORMAT_D16_UNORM;
		case Format::d24_unorm_s8_uint: return VK_FORMAT_D24_UNORM_S8_UINT;
		case Format::d32_float: return VK_FORMAT_D32_SFLOAT;
		case Format::s8_uint: return VK_FORMAT_S8_UINT;
		case Format::d32_float_s8_uint: return VK_FORMAT_D32_SFLOAT_S8_UINT;
		case Format::eac_rg: return VK_FORMAT_EAC_R11G11_UNORM_BLOCK;
		case Format::astc_4x4_srgb: return VK_FORMAT_ASTC_4x4_SRGB_BLOCK;
		case Format::astc_4x4_unorm: return VK_FORMAT_ASTC_4x4_UNORM_BLOCK;
		case Format::bc3_srgb: return VK_FORMAT_BC3_SRGB_BLOCK;
		case Format::bc3_unorm: return VK_FORMAT_BC3_UNORM_BLOCK;
		case Format::bc5_rg: return VK_FORMAT_BC5_UNORM_BLOCK;
		case Format::bc6h_ufloat: return VK_FORMAT_BC6H_UFLOAT_BLOCK;
		case Format::bc6h_sfloat: return VK_FORMAT_BC6H_SFLOAT_BLOCK;
		case Format::bc7_srgb: return VK_FORMAT_BC7_SRGB_BLOCK;
		case Format::bc7_unorm: return VK_FORMAT_BC7_UNORM_BLOCK;
		case Format::undefined: return VK_FORMAT_UNDEFINED;
		}
		return {};
	}

	VkBlendFactor toVk(BlendFactor factor)
	{
		switch (factor)
		{
		case BlendFactor::zero: return VK_BLEND_FACTOR_ZERO;
		case BlendFactor::one: return VK_BLEND_FACTOR_ONE;
		case BlendFactor::source_color: return VK_BLEND_FACTOR_SRC_COLOR;
		case BlendFactor::one_minus_source_color: return VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
		case BlendFactor::destination_color: return VK_BLEND_FACTOR_DST_COLOR;
		case BlendFactor::one_minus_destination_color: return VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
		case BlendFactor::source_alpha: return VK_BLEND_FACTOR_SRC_ALPHA;
		case BlendFactor::one_minus_source_alpha: return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		case BlendFactor::destination_alpha: return VK_BLEND_FACTOR_DST_ALPHA;
		case BlendFactor::one_minus_destination_alpha: return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
		case BlendFactor::source_alpha_saturate: return VK_BLEND_FACTOR_SRC_ALPHA_SATURATE;
		}
		return VK_BLEND_FACTOR_MAX_ENUM;
	}

	VkResult Device::createPresentContext(PresentContext& context)
	{
		//assert(!context.acquired && !context.rendered && !context.presented && !context.swapchain && !context.present_pending);
		const VkSemaphoreCreateInfo semaphoreInfo{
			.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
		};
		VkResult error = vkCreateSemaphore(device, &semaphoreInfo, nullptr, &context.acquired);
		if (error != VK_SUCCESS)
			return error;
		error = vkCreateSemaphore(device, &semaphoreInfo, nullptr, &context.rendered);
		if (error != VK_SUCCESS)
		{
			destroyPresentContext(context);
			return error;
		}
		const VkFenceCreateInfo fenceInfo{
			.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
		};
		error = vkCreateFence(device, &fenceInfo, nullptr, &context.presented);
		if (error != VK_SUCCESS)
		{
			destroyPresentContext(context);
			return error;
		}
		return VK_SUCCESS;
	}

	VkCompositeAlphaFlagBitsKHR chooseCompositeAlpha(VkCompositeAlphaFlagsKHR supported)
	{
		constexpr VkCompositeAlphaFlagBitsKHR choices[]{
			VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
			VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
			VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR,
			VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR,
		};
		for (VkCompositeAlphaFlagBitsKHR choice : choices)
		{
			if ((supported & choice) != 0)
				return choice;
		}
		return static_cast<VkCompositeAlphaFlagBitsKHR>(0);
	}

	// If a change is called, and the current/old swapchain is no longer suitable, delete that handle
	void retireSwapchainHandle(Swapchain& swapchain) noexcept
	{
		Device* device = swapchain.state;
		if (!swapchain.handle)
		{
			//assert(swapchain.image_count == 0);
			swapchain.width = 0;
			swapchain.height = 0;
			return;
		}

		device->pollPresentContexts();
		RetiredSwapchain retired{
			.handle = swapchain.handle,
		};
		for (uint32_t index = 0; index < swapchain.imageCount; ++index)
		{
			RenderView& render_view = swapchain.renderViews[index];
			retired.views[retired.viewCount++] = render_view.view;
			render_view = {};
			swapchain.images[index] = VK_NULL_HANDLE;
			swapchain.initialized[index] = false;
		}
		swapchain.imageCount = 0;
		swapchain.handle = VK_NULL_HANDLE;
		swapchain.width = 0;
		swapchain.height = 0;

		bool present_pending = false;
		for (uint32_t index = 0; index < device->presentContextCount; ++index)
		{
			const PresentContext& context = device->presentContexts[index];
			if (context.presentPending && context.swapchain == retired.handle)
			{
				present_pending = true;
				break;
			}
		}
		if (!present_pending)
		{
			device->queueRetiredSwapchain(retired);
			return;
		}
		for (RetiredSwapchain& slot : device->retiredSwapchains)
		{
			if (!slot.handle)
			{
				slot = retired;
				return;
			}
		}
		for (uint32_t index = 0; index < device->presentContextCount; ++index)
		{
			PresentContext& context = device->presentContexts[index];
			if (context.presentPending && context.swapchain == retired.handle)
				device->waitPresentContext(context);
		}
		device->queueRetiredSwapchain(retired);
	}

	// If a change is needed for the swapchain, or for initial creation, we recreate the swapchain here
	VkResult recreateSwapchain(Swapchain& swapchain)
	{
		Device& device = *swapchain.state;
		//assert(!swapchain.acquired && !device.acquired_swapchain);
		const VkSurfacePresentModeKHR presentModeInfo{
			.sType = VK_STRUCTURE_TYPE_SURFACE_PRESENT_MODE_KHR,
			.presentMode = presentMode,
		};
		const VkPhysicalDeviceSurfaceInfo2KHR surfaceInfo{
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SURFACE_INFO_2_KHR,
			.pNext = &presentModeInfo,
			.surface = device.surface,
		};
		VkSurfaceCapabilities2KHR capabilitiesInfo{
			.sType = VK_STRUCTURE_TYPE_SURFACE_CAPABILITIES_2_KHR,
		};
		VkResult error = vkGetPhysicalDeviceSurfaceCapabilities2KHR(device.physicalDevice, &surfaceInfo, &capabilitiesInfo);
		if (error != VK_SUCCESS)
			return error;
		const VkSurfaceCapabilitiesKHR& capabilities = capabilitiesInfo.surfaceCapabilities;

		const VkExtent2D extent = capabilities.currentExtent;
		if (extent.width == UINT_MAX)
			return VK_INCOMPLETE;
		if ((capabilities.supportedUsageFlags & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) == 0)
		{
			return VK_INCOMPLETE;
		}

		VkSurfaceFormatKHR formats[maxSurfaceFormats]{};
		uint32_t surfaceFormatCount = 0;
		error = vkGetPhysicalDeviceSurfaceFormatsKHR(device.physicalDevice, device.surface, &surfaceFormatCount, nullptr);
		if (error != VK_SUCCESS)
			return error;
		if (surfaceFormatCount == 0 || surfaceFormatCount > maxSurfaceFormats)
			return VK_INCOMPLETE;
		error = vkGetPhysicalDeviceSurfaceFormatsKHR(device.physicalDevice, device.surface, &surfaceFormatCount, formats);
		if (error != VK_SUCCESS)
			return error;

		const VkFormat requestedFormat = toVk(swapchain.format);
		bool formatSupported = false;
		for (uint32_t index = 0; index < surfaceFormatCount; ++index)
		{
			if ((formats[index].format == requestedFormat ||
				formats[index].format == VK_FORMAT_UNDEFINED) &&
				formats[index].colorSpace == swapchain.colorSpace)
			{
				formatSupported = true;
				break;
			}
		}
		if (!formatSupported)
			return VK_INCOMPLETE;

		if (extent.width == 0 || extent.height == 0)
		{
			swapchain.width = 0;
			swapchain.height = 0;
			swapchain.recreateRequired = true;
			return VK_ERROR_INITIALIZATION_FAILED;
		}

		uint32_t requestedImageCount = device.presentContextCount;
		if (requestedImageCount < capabilities.minImageCount) requestedImageCount = capabilities.minImageCount;
		if (capabilities.maxImageCount != 0 && requestedImageCount > capabilities.maxImageCount) requestedImageCount = capabilities.maxImageCount;
		if (requestedImageCount == 0 || requestedImageCount > maxSwapchainImages) return VK_ERROR_INITIALIZATION_FAILED;

		const VkCompositeAlphaFlagBitsKHR compositeAlpha = chooseCompositeAlpha(capabilities.supportedCompositeAlpha);
		if (!compositeAlpha)
		{
			//if (error_callback) error_callback("The display surface exposes no supported composite alpha mode.", false);
			return VK_INCOMPLETE;
		}
		const VkSwapchainKHR oldHandle = swapchain.handle;
		const VkSwapchainPresentModesCreateInfoKHR presentModesInfo{
			.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_PRESENT_MODES_CREATE_INFO_KHR,
			.presentModeCount = 1,
			.pPresentModes = &presentMode,
		};
		const VkSwapchainCreateInfoKHR createInfo{
			.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
			.pNext = &presentModesInfo,
			.surface = device.surface,
			.minImageCount = requestedImageCount,
			.imageFormat = requestedFormat,
			.imageColorSpace = swapchain.colorSpace,
			.imageExtent = extent,
			.imageArrayLayers = 1,
			.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
			.imageSharingMode = device.queueFamilyCount > 1 ? VK_SHARING_MODE_CONCURRENT : VK_SHARING_MODE_EXCLUSIVE,
			.queueFamilyIndexCount = device.queueFamilyCount,
			.pQueueFamilyIndices = device.queueFamiliesIndex,
			.preTransform = capabilities.currentTransform,
			.compositeAlpha = compositeAlpha,
			.presentMode = presentMode,
			.clipped = VK_TRUE,
			.oldSwapchain = oldHandle,
		};
		VkSwapchainKHR newHandle = VK_NULL_HANDLE;
		error = vkCreateSwapchainKHR(device.device, &createInfo, nullptr, &newHandle);
		if (error != VK_SUCCESS)
		{
			retireSwapchainHandle(swapchain);
			return error;
		}

		VkImage images[maxSwapchainImages]{};
		uint32_t imageCount = 0;
		VkResult result = vkGetSwapchainImagesKHR(device.device, newHandle, &imageCount, nullptr);
		if (result != VK_SUCCESS || imageCount == 0 || imageCount > maxSwapchainImages)
		{
			vkDestroySwapchainKHR(device.device, newHandle, nullptr);
			retireSwapchainHandle(swapchain);
			return result;
		}
		result = vkGetSwapchainImagesKHR(device.device, newHandle, &imageCount, images);
		if (result != VK_SUCCESS)
		{
			vkDestroySwapchainKHR(device.device, newHandle, nullptr);
			retireSwapchainHandle(swapchain);
			return result;
		}

		VkImageView views[maxSwapchainImages]{};
		for (uint32_t index = 0; index < imageCount; ++index)
		{
			const VkImageViewCreateInfo view_info{
				.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
				.image = images[index],
				.viewType = VK_IMAGE_VIEW_TYPE_2D,
				.format = requestedFormat,
				.subresourceRange = {
					.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
					.levelCount = 1,
					.layerCount = 1,
				},
			};
			result = vkCreateImageView(device.device, &view_info, nullptr, &views[index]);
			if (result != VK_SUCCESS)
			{
				for (uint32_t created = 0; created < index; ++created)
				{
					vkDestroyImageView(device.device, views[created], nullptr);
				}
				vkDestroySwapchainKHR(device.device, newHandle, nullptr);
				retireSwapchainHandle(swapchain);
				return result;
			}
		}

		retireSwapchainHandle(swapchain);
		swapchain.handle = newHandle;
		swapchain.imageCount = imageCount;
		swapchain.width = extent.width;
		swapchain.height = extent.height;
		swapchain.transform = capabilities.currentTransform;
		swapchain.compositeAlpha = compositeAlpha;
		swapchain.recreateRequired = false;
		for (uint32_t index = 0; index < imageCount; ++index)
		{
			swapchain.images[index] = images[index];
			swapchain.renderViews[index] = {
				.state = &device,
				.view = views[index],
				.width = extent.width,
				.height = extent.height,
				.swapchainView = true,
			};
		}
		return VK_SUCCESS;
	}

	void setViewport(CommandBuffer* commands, const Viewport& viewport)
	{
		const VkViewport vkViewport{
			.x = viewport.x,
			.y = viewport.y,
			.width = viewport.width,
			.height = viewport.height,
			.minDepth = viewport.minDepth,
			.maxDepth = viewport.maxDepth,
		};
		vkCmdSetViewportWithCount(commands->commandBuffer, 1, &vkViewport);
	}

	void setScissor(CommandBuffer* commands, const Scissor& scissor)
	{
		const VkRect2D vkScissor{
			.offset = { .x = scissor.x, .y = scissor.y },
			.extent = { .width = scissor.width, .height = scissor.height },
		};
		vkCmdSetScissorWithCount(commands->commandBuffer, 1, &vkScissor);
	}

	void setDepthStencil(CommandBuffer* commands, const DepthStencilState& state)
	{
		vkCmdSetDepthTestEnable(commands->commandBuffer, state.depthTest);
		if (state.depthTest)
		{
			vkCmdSetDepthWriteEnable(commands->commandBuffer, state.depthWrite);
			vkCmdSetDepthCompareOp(commands->commandBuffer, static_cast<VkCompareOp>(state.depth_compare));
		}
		vkCmdSetStencilTestEnable(commands->commandBuffer, state.stencil_test);
		if (!state.stencil_test)
			return;

		vkCmdSetStencilOp(commands->commandBuffer, VK_STENCIL_FACE_FRONT_BIT,
			static_cast<VkStencilOp>(state.front.fail), static_cast<VkStencilOp>(state.front.pass),
			static_cast<VkStencilOp>(state.front.depth_fail), static_cast<VkCompareOp>(state.front.compare));
		vkCmdSetStencilOp(commands->commandBuffer, VK_STENCIL_FACE_BACK_BIT,
			static_cast<VkStencilOp>(state.back.fail), static_cast<VkStencilOp>(state.back.pass),
			static_cast<VkStencilOp>(state.back.depth_fail), static_cast<VkCompareOp>(state.back.compare));
		vkCmdSetStencilCompareMask(commands->commandBuffer, VK_STENCIL_FACE_FRONT_AND_BACK, state.stencil_read_mask);
		vkCmdSetStencilWriteMask(commands->commandBuffer, VK_STENCIL_FACE_FRONT_AND_BACK, state.stencil_write_mask);
		vkCmdSetStencilReference(commands->commandBuffer, VK_STENCIL_FACE_FRONT_BIT, state.front.reference);
		vkCmdSetStencilReference(commands->commandBuffer, VK_STENCIL_FACE_BACK_BIT, state.back.reference);
	}

	void gpuBeginRenderPass(CommandBuffer* buffer, const RenderingDesc& desc, RenderingFlags flags)
	{
		const RenderView* areaView = desc.colors.size() ? desc.colors.at(0).renderView
			: (desc.depth.renderView ? desc.depth.renderView : desc.stencil.renderView);
		VkRenderingAttachmentInfo colorAttatchments[maxColorAttachments];
		for (size_t index = 0; index < desc.colors.size(); ++index)
		{
			const ColorAttachment& attachment = desc.colors.at(index);
			colorAttatchments[index] = {
				.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
				.imageView = attachment.renderView->view,
				.imageLayout = VK_IMAGE_LAYOUT_GENERAL,
				.loadOp = static_cast<VkAttachmentLoadOp>(attachment.load),
				.storeOp = static_cast<VkAttachmentStoreOp>(attachment.store),
				.clearValue = {
					.color = {
						.float32 = { attachment.clear.r, attachment.clear.g, attachment.clear.b, attachment.clear.a },
					},
				},
			};
		}

		const VkRenderingAttachmentInfo depthAttachment{
			.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
			.imageView = desc.depth.renderView ? desc.depth.renderView->view : VK_NULL_HANDLE,
			.imageLayout = VK_IMAGE_LAYOUT_GENERAL,
			.loadOp = static_cast<VkAttachmentLoadOp>(desc.depth.load),
			.storeOp = static_cast<VkAttachmentStoreOp>(desc.depth.store),
			.clearValue = {
				.depthStencil = {
					.depth = desc.depth.clear,
				},
			},
		};
		const VkRenderingAttachmentInfo stencilAttachment{
			.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
			.imageView = desc.stencil.renderView ? desc.stencil.renderView->view : VK_NULL_HANDLE,
			.imageLayout = VK_IMAGE_LAYOUT_GENERAL,
			.loadOp = static_cast<VkAttachmentLoadOp>(desc.stencil.load),
			.storeOp = static_cast<VkAttachmentStoreOp>(desc.stencil.store),
			.clearValue = {
				.depthStencil = {
					.stencil = desc.stencil.clear,
				},
			},
		};


		const VkRenderingInfo renderingInfo{
			.sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
			.flags = static_cast<VkRenderingFlags>(flags),
			.renderArea = {
				.extent = { .width = areaView->width, .height = areaView->height },
			},
			.layerCount = 1,
			.colorAttachmentCount = static_cast<uint32_t>(desc.colors.size()),
			.pColorAttachments = colorAttatchments,
			.pDepthAttachment = desc.depth.renderView ? &depthAttachment : nullptr,
			.pStencilAttachment = desc.stencil.renderView ? &stencilAttachment : nullptr,
		};



		// Render the info sent to command buffer
		vkCmdBeginRendering(buffer->commandBuffer, &renderingInfo);

		setViewport(buffer, { .width = static_cast<float>(areaView->width), .height = static_cast<float>(areaView->height) });
		setScissor(buffer, { .width = areaView->width, .height = areaView->height });
		setDepthStencil(buffer, {});
	}

	void gpuEndRenderPass(CommandBuffer* buffer)
	{
		vkCmdEndRendering(buffer->commandBuffer);
	}

	void draw(CommandBuffer* commands, const void* root, uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance)
	{
		emitRootPointer(commands, root);
		vkCmdDraw(commands->commandBuffer, vertexCount, instanceCount, firstVertex, firstInstance);
	}

	void submitCommands(Device* device, const SubmitDesc& desc, uint32_t queue_index, VkSemaphore wait_semaphore, VkSemaphore signal_semaphore)
	{
		Queue* queue = &device->queueArray[queue_index];
		TimelineSemaphore* completion = desc.completion.semaphore;
		//assert(completion);
		//assert(desc.waits.data || desc.waits.size == 0);
		size_t command_count = desc.commands.size();
		for (size_t index = 0; index < desc.commands.size(); ++index)
		{
			//assert(desc.commands.data[index]);
			if (desc.commands.at(index)->hasEpilogue) ++command_count;
		}
		if (command_count > queue->commandSubmitCapacity)
		{
			queue->commandSubmitCapacity = queue->commandSubmitCapacity == 0 ? 4 : queue->commandSubmitCapacity * 2;
			if (queue->commandSubmitCapacity < command_count) queue->commandSubmitCapacity = command_count;
			queue->commandSubmitInfos = static_cast<VkCommandBufferSubmitInfo*>(
				realloc(queue->commandSubmitInfos, queue->commandSubmitCapacity * sizeof(VkCommandBufferSubmitInfo)));
		}
		for (size_t index = 0; index < desc.commands.size(); ++index)
		{
			CommandBuffer* commands = desc.commands.at(index);
			//assert(commands);
			queue->commandSubmitInfos[index] = {
				.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
				.commandBuffer = commands->commandBuffer,
				.deviceMask = 1,
			};
		}
		// Presentation transitions must follow every suspended/resumed segment.
		size_t epilogue_index = desc.commands.size();
		for (size_t index = 0; index < desc.commands.size(); ++index)
		{
			CommandBuffer* commands = desc.commands.at(index);
			if (commands->hasEpilogue)
			{
				queue->commandSubmitInfos[epilogue_index++] = {
					.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
					.commandBuffer = commands->epilogue,
					.deviceMask = 1,
				};
			}
		}
		const size_t wait_count = desc.waits.size() + (wait_semaphore ? 1 : 0);
		if (wait_count > queue->waitSubmitCapacity)
		{
			queue->waitSubmitCapacity = queue->waitSubmitCapacity == 0 ? 4 : queue->waitSubmitCapacity * 2;
			if (queue->waitSubmitCapacity < wait_count) queue->waitSubmitCapacity = wait_count;
			queue->waitSubmitInfos = static_cast<VkSemaphoreSubmitInfo*>(
				realloc(queue->waitSubmitInfos, queue->waitSubmitCapacity * sizeof(VkSemaphoreSubmitInfo)));
		}
		for (size_t index = 0; index < desc.waits.size(); ++index)
		{
			const TimelinePoint point = desc.waits.at(index);
			//assert(point.semaphore);
			queue->waitSubmitInfos[index] = {
				.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
				.semaphore = point.semaphore->semaphore,
				.value = point.value,
				.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
			};
		}
		if (wait_semaphore)
		{
			queue->waitSubmitInfos[desc.waits.size()] = {
				.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
				.semaphore = wait_semaphore,
				.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
			};
		}
		VkSemaphoreSubmitInfo signal_infos[3]{
			{
				.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
				.semaphore = completion->semaphore,
				.value = desc.completion.value,
				.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
			},
		};
		uint32_t signal_count = 1;
		if (signal_semaphore)
		{
			signal_infos[signal_count++] = {
				.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
				.semaphore = signal_semaphore,
				.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
			};
			signal_infos[signal_count++] = {
				.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
				.semaphore = device->presentationRetirement,
				.value = device->nextPresentationRetirement(),
				.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
			};
		}
		const VkSubmitInfo2 submit_info{
			.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
			.waitSemaphoreInfoCount = static_cast<uint32_t>(wait_count),
			.pWaitSemaphoreInfos = queue->waitSubmitInfos,
			.commandBufferInfoCount = static_cast<uint32_t>(command_count),
			.pCommandBufferInfos = queue->commandSubmitInfos,
			.signalSemaphoreInfoCount = signal_count,
			.pSignalSemaphoreInfos = signal_infos,
		};
		vkQueueSubmit2(queue->queue, 1, &submit_info, VK_NULL_HANDLE);
		for (size_t index = 0; index < desc.commands.size(); ++index) desc.commands.at(index)->submitted = true;
	}

	void submitAndPresent(Device* device, const SubmitDesc& desc)
	{
		Swapchain* swapchain = device->swapchain;
		PresentContext& presentContext = *swapchain->presentContext; // Swapchain not creating present contexts its created in device!?!?!

		VkResult resetFenceResult = vkResetFences(device->device, 1, &presentContext.presented);
		if (resetFenceResult != VK_SUCCESS)
		{
			std::cout << "Failed to reset fence. Error Code: " << resetFenceResult << std::endl;
		}
		submitCommands(device, desc, 0, presentContext.acquired, presentContext.rendered);

		swapchain->transitionCommands->swapchain = nullptr;
		swapchain->transitionCommands = nullptr;
		swapchain->initialized[swapchain->imageIndex] = true;

		const VkSwapchainPresentFenceInfoKHR fenceInfo{
			.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_PRESENT_FENCE_INFO_KHR,
			.swapchainCount = 1,
			.pFences = &presentContext.presented,
		};
		const VkPresentInfoKHR presentInfo{
			.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
			.pNext = &fenceInfo,
			.waitSemaphoreCount = 1,
			.pWaitSemaphores = &presentContext.rendered,
			.swapchainCount = 1,
			.pSwapchains = &swapchain->handle,
			.pImageIndices = &swapchain->imageIndex,
		};
		const VkResult result = vkQueuePresentKHR(device->queueArray[0].queue, &presentInfo);
		presentContext.presentPending = true;
		presentContext.swapchain = swapchain->handle;
		swapchain->presentContext = nullptr;
		swapchain->acquired = false;
		device->acquiredSwapchain = nullptr;
		device->nextPresentContext = (device->nextPresentContext + 1) % device->presentContextCount;
		if (result == VK_ERROR_OUT_OF_DATE_KHR)
			swapchain->recreateRequired = true;
		else if (result == VK_SUBOPTIMAL_KHR)
			swapchain->recreateRequired = swapchain->recreateRequired || swapchainSurfaceConfigurationChanged(*swapchain);
		else if (result != VK_SUCCESS)
			throw std::runtime_error("FAILED TO PRESENT swapchain images");
	}

	void emitRootPointer(CommandBuffer* commands, const void* root)
	{
		if (!root) return;
		const VkPushDataInfoEXT info{
			.sType = VK_STRUCTURE_TYPE_PUSH_DATA_INFO_EXT,
			.data = { .address = &root, .size = sizeof(root) },
		};
		commands->state->functions.cmdPushData(commands->commandBuffer, &info);
	}

	SwapchainFrame acquire(CommandBuffer* commands)
	{
		Device* device = commands->state;
		device->pollPresentationRetirement();


		Swapchain* swapchain = device->swapchain;
		PresentContext* present_context = nullptr;

		for (;;)
		{
			if (!swapchain->handle || swapchain->recreateRequired)
			{
				VkResult error = recreateSwapchain(*swapchain);
				if (error != VK_SUCCESS)
				{
					throw std::runtime_error("Failed to recreate swapchain");
				}
				const bool drawable = swapchain->handle && swapchain->width != 0 && swapchain->height != 0;
				if (!drawable)
					return {};
			}

			if (!present_context)
			{
				present_context = &device->presentContexts[device->nextPresentContext];
				device->waitPresentContext(*present_context);
				//assert(!present_context->present_pending && !present_context->swapchain);
			}

			uint32_t image_index = 0;
			const VkResult result = vkAcquireNextImageKHR(
				device->device,
				swapchain->handle,
				~uint64_t{ 0 },
				present_context->acquired,
				VK_NULL_HANDLE,
				&image_index);
			if (result == VK_ERROR_OUT_OF_DATE_KHR)
			{
				swapchain->recreateRequired = true;
				continue;
			}
			if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
				throw std::runtime_error("Acquire frame failure");
			//assert((image_index < swapchain->image_count) && "swapchain returned an invalid image index");

			swapchain->imageIndex = image_index;
			swapchain->presentContext = present_context;
			swapchain->acquired = true;
			swapchain->recreateRequired = result == VK_SUBOPTIMAL_KHR && swapchainSurfaceConfigurationChanged(*swapchain);
			device->acquiredSwapchain = swapchain;
			const VkImageMemoryBarrier2 barrier{
				.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
				.srcStageMask = VK_PIPELINE_STAGE_2_NONE,
				.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
				.dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT,
				.oldLayout = swapchain->initialized[image_index] ? VK_IMAGE_LAYOUT_PRESENT_SRC_KHR : VK_IMAGE_LAYOUT_UNDEFINED,
				.newLayout = VK_IMAGE_LAYOUT_GENERAL,
				.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
				.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
				.image = swapchain->images[image_index],
				.subresourceRange = {
					.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
					.levelCount = 1,
					.layerCount = 1,
				},
			};
			recordImageBarriers(commands->commandBuffer, &barrier, 1);
			commands->swapchain = swapchain;
			swapchain->transitionCommands = commands;
			return {
				.renderView = &swapchain->renderViews[image_index],
				.extent = { .x = swapchain->width, .y = swapchain->height },
			};
		}
	}

	bool swapchainSurfaceConfigurationChanged(const Swapchain& swapchain)
	{
		VkSurfaceCapabilitiesKHR capabilities{};
		VkResult error = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(swapchain.state->physicalDevice, swapchain.state->surface, &capabilities);
		if (error != VK_SUCCESS)
		{
			return false;
		}

		const VkExtent2D extent = capabilities.currentExtent;
		const uint32_t variable_extent = UINT_MAX;
		return extent.width == variable_extent ||
			extent.height == variable_extent ||
			extent.width != swapchain.width ||
			extent.height != swapchain.height ||
			capabilities.currentTransform != swapchain.transform ||
			chooseCompositeAlpha(capabilities.supportedCompositeAlpha) != swapchain.compositeAlpha;
	}

	// Format related methods
	VkFormatFeatureFlags2 optimalFormatFeatures(VkPhysicalDevice physicalDevice, Format format)
	{
		VkFormatProperties3 properties3{
			.sType = VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_3,
		};
		VkFormatProperties2 properties2{
			.sType = VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2,
			.pNext = &properties3,
		};
		vkGetPhysicalDeviceFormatProperties2(physicalDevice, toVk(format), &properties2);
		return properties3.optimalTilingFeatures;
	}

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
		const bool addressCommandsExtension = hasName(supportedDeviceExtensions, VK_KHR_DEVICE_ADDRESS_COMMANDS_EXTENSION_NAME); // TODO, seemingly returning true when not supposed too
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
		if (result.queueCounts[0] == 0) // TODO: add other checks here
		{
			return VK_INCOMPLETE;
		}
		result.enabledExtensions = enabledDeviceExtensions;
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
		device->timestampQueryCount = desc.timestampQueryCount;
		device->presentContextCount = true ? desc.desiredSwapchainImageCount : 0;
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
		Candidate selected{};
		for (const VkPhysicalDevice& physicalDevice : physicalDeviceList)
		{
			Candidate candidate{};
			if (inspectDevice(physicalDevice, desc, foundMaintenanceName, device->surface, candidate) == VK_SUCCESS)
			{
				selected = candidate;
				break;
			}
		}

		// Set device properties
		// TODO: add format picking
		device->physicalDevice	 = selected.physicalDevice;
		device->deviceProperties = selected.properties;
		device->deviceHeapProps  = selected.heapProperties;
		device->deviceMemProps   = selected.memoryProperties;
		device->maxTimelineValueDifference = selected.vulkan12Properties.maxTimelineSemaphoreValueDifference;
		device->deviceHeapProps.pNext = nullptr;
		// Find the best formats we have avaiable on the system
		for (uint32_t value = 0; value < formatCount; ++value)
		{
			device->formatFeatures[value] = optimalFormatFeatures(
				device->physicalDevice, static_cast<Format>(value));
		}


		DesiredFeatures enabledFeatures(true, selected.unifiedImageLayouts, selected.addressCommands);
		device->textureCompressionEtc2 = selected.textureCompressionEtc2;
		enabledFeatures.core.features.imageCubeArray = selected.imageCubeArray;
		enabledFeatures.core.features.samplerAnisotropy = VK_TRUE;
		enabledFeatures.core.features.shaderInt16 = VK_TRUE;
		enabledFeatures.core.features.depthBiasClamp = VK_TRUE;
		enabledFeatures.core.features.independentBlend = VK_TRUE;
		enabledFeatures.core.features.textureCompressionBC = selected.textureCompressionBc;
		enabledFeatures.core.features.textureCompressionASTC_LDR = selected.textureCompressionAstc;
		enabledFeatures.core.features.textureCompressionETC2 = selected.textureCompressionEtc2;
		enabledFeatures.core.features.fragmentStoresAndAtomics = VK_TRUE;
		enabledFeatures.core.features.vertexPipelineStoresAndAtomics = VK_TRUE;
		enabledFeatures.core.features.shaderStorageImageReadWithoutFormat = VK_TRUE;
		enabledFeatures.core.features.shaderStorageImageWriteWithoutFormat = VK_TRUE;
		enabledFeatures.core.features.multiDrawIndirect = VK_TRUE;
		enabledFeatures.core.features.drawIndirectFirstInstance = VK_TRUE;
		enabledFeatures.vulkan11.storageBuffer16BitAccess = VK_TRUE;
		enabledFeatures.vulkan11.storagePushConstant16 = VK_TRUE;
		enabledFeatures.vulkan11.storageInputOutput16 = selected.storageInputOutput16;
		enabledFeatures.vulkan11.shaderDrawParameters = VK_TRUE;
		enabledFeatures.vulkan12.shaderFloat16 = VK_TRUE;
		enabledFeatures.vulkan12.scalarBlockLayout = VK_TRUE;
		enabledFeatures.vulkan12.timelineSemaphore = VK_TRUE;
		enabledFeatures.vulkan12.hostQueryReset = selected.timestamps[0] || selected.timestamps[1] || selected.timestamps[2];
		enabledFeatures.vulkan12.bufferDeviceAddress = VK_TRUE;
		enabledFeatures.vulkan13.synchronization2 = VK_TRUE;
		enabledFeatures.vulkan13.dynamicRendering = VK_TRUE;
		enabledFeatures.vulkan13.maintenance4 = VK_TRUE;
		enabledFeatures.vulkan14.maintenance5 = VK_TRUE;
		enabledFeatures.descriptorHeap.descriptorHeap = VK_TRUE;
		enabledFeatures.addressCommands.deviceAddressCommands = selected.addressCommands;
		enabledFeatures.untypedPointers.shaderUntypedPointers = VK_TRUE;
		enabledFeatures.unifiedImageLayouts.unifiedImageLayouts = selected.unifiedImageLayouts ? VK_TRUE : VK_FALSE;
		enabledFeatures.meshShader.taskShader = VK_TRUE;
		enabledFeatures.meshShader.meshShader = VK_TRUE;
		enabledFeatures.swapchainMaintenance1.swapchainMaintenance1 = VK_TRUE;

		// Get queue information from each assigned family
		const uint32_t requestedQueueCounts[]{ desc.desiredGraphicsQueues, desc.desiredComputeQueues, desc.desiredTransferQueues };
		uint32_t queueCountsOfTypes[queueTypeCount]{};
		for (uint32_t type = 0; type < queueTypeCount; ++type)
		{
			queueCountsOfTypes[type] = requestedQueueCounts[type] < selected.queueCounts[type] ? requestedQueueCounts[type] : selected.queueCounts[type];
			device->totalQueueCount += queueCountsOfTypes[type];
		}

		device->queueArray = new Queue[device->totalQueueCount];
		float* queueProrities = new float[device->totalQueueCount];
		for (uint32_t index = 0; index < device->totalQueueCount; ++index)
			queueProrities[index] = 1.0f; // Start at highest priority?

		// Create the create info structs for our new queues
		// NOTE: create info goes by families, hence why we only have 3 create infos from the total queue types
		// TODO: something wrong with queue Create info
		VkDeviceQueueCreateInfo queueCreateInfos[queueTypeCount]{};
		uint32_t firstQueue = 0;
		for (uint32_t type = 0; type < queueTypeCount; ++type)
		{
			if (queueCountsOfTypes[type] == 0) continue;
			device->queueFamiliesIndex[device->queueFamilyCount] = selected.queueFamilies[type];
			queueCreateInfos[device->queueFamilyCount++] =
			{
				.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
				.queueFamilyIndex = selected.queueFamilies[type],
				.queueCount = queueCountsOfTypes[type],
				.pQueuePriorities = queueProrities + firstQueue,
			};
			firstQueue += queueCountsOfTypes[type]; // Increase priority for each later queue?!?!? supposedly priority goes from 0.0-1.0(lowest to highest)
		}

		
		// TODO: queueCreateInfos seems to be creating 3 queue families because of the const queueTypeCount when we only are using 1 family in this case
		// Potentiallialy remove? It works however since default of .pNext is nullptr I believe
		// Get device info and create device
		VkDeviceCreateInfo deviceInfo
		{
			.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
			.pNext = &enabledFeatures.core,
			.queueCreateInfoCount = device->queueFamilyCount,
			.pQueueCreateInfos = queueCreateInfos,
			.enabledExtensionCount = (uint32_t) selected.enabledExtensions.size(),
			.ppEnabledExtensionNames = selected.enabledExtensions.data(),
		};
		VkResult deviceCreationResult = vkCreateDevice(device->physicalDevice, &deviceInfo, nullptr, &device->device);
		if (deviceCreationResult != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create logical device!");
		}
		delete[] queueProrities;

		// Get that queue Info
		firstQueue = 0;
		for (uint32_t type = 0; type < queueTypeCount; ++type)
		{
			for (uint32_t index = 0; index < queueCountsOfTypes[type]; ++index)
			{
				Queue& queue = device->queueArray[firstQueue++];
				queue.familyIndex = selected.queueFamilies[type];
				queue.timestamps = selected.timestamps[type];
				vkGetDeviceQueue(device->device, queue.familyIndex, index, &queue.queue); // TODO: make this vkGetDeviceQueue2
			}
		}
		// TODO: Get this done, not needed immediately hence why I skipped it
		//if (!supports_gpu_heap_memory(*state) || !select_texture_memory_type(*state))
		//	return fail_device_creation(state, Error::unsupported);

		// Fill in device functions
		device->functions.writeSamplerDescriptors = loadDeviceProc<PFN_vkWriteSamplerDescriptorsEXT>(device->device, "vkWriteSamplerDescriptorsEXT");
		device->functions.writeResourceDescriptors = loadDeviceProc<PFN_vkWriteResourceDescriptorsEXT>(device->device, "vkWriteResourceDescriptorsEXT");
		device->functions.cmdBindSamplerHeap = loadDeviceProc<PFN_vkCmdBindSamplerHeapEXT>(device->device, "vkCmdBindSamplerHeapEXT");
		device->functions.cmdBindTextureHeap = loadDeviceProc<PFN_vkCmdBindResourceHeapEXT>(device->device, "vkCmdBindResourceHeapEXT");
		device->functions.cmdPushData = loadDeviceProc<PFN_vkCmdPushDataEXT>(device->device, "vkCmdPushDataEXT");
		device->functions.cmdDrawMeshTasks = loadDeviceProc<PFN_vkCmdDrawMeshTasksEXT>(device->device, "vkCmdDrawMeshTasksEXT");
		if (!device->functions.writeSamplerDescriptors || !device->functions.writeResourceDescriptors ||
			!device->functions.cmdBindSamplerHeap || !device->functions.cmdBindTextureHeap ||
			!device->functions.cmdPushData || !device->functions.cmdDrawMeshTasks)
		{
			throw std::runtime_error("Failure to get required device functions!");
		}
		if (selected.addressCommands == false) // NOTE: momentarily disabling addressCommands as my GPU seems to not have ANY of these features
		{
			device->functions.cmdBindIndexBuffer = loadDeviceProc<PFN_vkCmdBindIndexBuffer3KHR>(device->device, "vkCmdBindIndexBuffer3KHR"); // THIS failed?
			device->functions.cmdDrawIndirect = loadDeviceProc<PFN_vkCmdDrawIndirect2KHR>(device->device, "vkCmdDrawIndirect2KHR");
			device->functions.cmdDrawIndexedIndirect = loadDeviceProc<PFN_vkCmdDrawIndexedIndirect2KHR>(device->device, "vkCmdDrawIndexedIndirect2KHR");
			device->functions.cmdDispatchIndirect = loadDeviceProc<PFN_vkCmdDispatchIndirect2KHR>(device->device, "vkCmdDispatchIndirect2KHR");
			device->functions.cmdDrawMeshTasksIndirect = loadDeviceProc<PFN_vkCmdDrawMeshTasksIndirect2EXT>(device->device, "vkCmdDrawMeshTasksIndirect2EXT");
			device->functions.cmdCopyMemory = loadDeviceProc<PFN_vkCmdCopyMemoryKHR>(device->device, "vkCmdCopyMemoryKHR");
			device->functions.cmdCopyMemoryToImage = loadDeviceProc<PFN_vkCmdCopyMemoryToImageKHR>(device->device, "vkCmdCopyMemoryToImageKHR");
			device->functions.cmdCopyImageToMemory = loadDeviceProc<PFN_vkCmdCopyImageToMemoryKHR>(device->device, "vkCmdCopyImageToMemoryKHR");
			if (!device->functions.cmdBindIndexBuffer || !device->functions.cmdDrawIndirect || !device->functions.cmdDrawIndexedIndirect ||
				!device->functions.cmdDispatchIndirect || !device->functions.cmdDrawMeshTasksIndirect || !device->functions.cmdCopyMemory ||
				!device->functions.cmdCopyMemoryToImage || !device->functions.cmdCopyImageToMemory)
			{
				throw std::runtime_error("Failure to get addressCommands device functions despite address commands supposedly being supported!");
			}
		}
		else
		{
			device->functions.cmdDrawMeshTasksIndirectBuffer = loadDeviceProc<PFN_vkCmdDrawMeshTasksIndirectEXT>(device->device, "vkCmdDrawMeshTasksIndirectEXT");
			if (!device->functions.cmdDrawMeshTasksIndirectBuffer)
				throw std::runtime_error("Failure to get addressCommands device functions despite address commands supposdely being supported!");
			//device->bufferMap = new detail::BufferAddressMap{}; TODO: Get bufferMap variable
		}

		const VkSemaphoreTypeCreateInfo typeInfo{
			.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
			.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE,
		};
		const VkSemaphoreCreateInfo semaphoreInfo{
			.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
			.pNext = &typeInfo,
		};
		VkResult semaphoreResult = vkCreateSemaphore(device->device, &semaphoreInfo, nullptr, &device->presentationRetirement);
		if (semaphoreResult != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create device semaphore!");
		}

		for (uint32_t index = 0; index < device->presentContextCount; ++index)
		{
			VkResult presentContextCreationResult = device->createPresentContext(device->presentContexts[index]);
			if (presentContextCreationResult != VK_SUCCESS)
				throw std::runtime_error("Failed to create present contexts");
		}

		device->specs = {
			.deviceName = device->deviceProperties.properties.deviceName,
			.queueCount = device->totalQueueCount,
			.graphicsQueueCount = queueCountsOfTypes[0],
			.computeQueueCount = queueCountsOfTypes[1],
			.transferQueueCount = queueCountsOfTypes[2],
			.copyTextureGranularity = selected.copyTextureGranularity,
			.maxPushDataSize = device->deviceHeapProps.maxPushDataSize,
			.textureHeapAlignment = device->textureHeapAlignment,
			.timestampPeriodns = selected.properties.properties.limits.timestampPeriod,
			.subTexelPrecisionBits = selected.properties.properties.limits.subTexelPrecisionBits,
			.textureCompressionBc = selected.textureCompressionBc,
			.textureCompressionAstc = selected.textureCompressionAstc,
			.storageInputOutput16 = selected.storageInputOutput16,
			.indirectMeshDraw = true,
		};

		device->swapchain = new Swapchain;
		device->swapchain->state = device;
		device->swapchain->format = desc.swapchainFormat;
		device->swapchain->colorSpace = desc.swapchainColorSpace == ColorSpace::extended_srgb_linear
			? VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT : VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
		VkResult swapchainCreationResult = recreateSwapchain(*device->swapchain);
		if (swapchainCreationResult != VK_SUCCESS)
			throw std::runtime_error("Failed to create swapchain!");

		// Create debug util

		return \
		{
			.newDevice = device
		};
	}
	VkResult createDebugUtil()
	{
		VkDebugUtilsMessengerCreateInfoEXT debugUitlInfo;

		return VkResult();
	}


	CommandPool* createCommandPool(Device* device, uint32_t queueIndex)
	{
		CommandPool* pool = new CommandPool{ .state = device, .timestamps = device->queueArray[queueIndex].timestamps };
		const VkCommandPoolCreateInfo poolInfo{
			.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
			.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
			.queueFamilyIndex = device->queueArray[queueIndex].familyIndex,
		};
		if (vkCreateCommandPool(device->device, &poolInfo, nullptr, &pool->commandPool) != VK_SUCCESS)
		{
			delete pool;
			return nullptr;
		}
		return pool;
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
			commands = new CommandBuffer{ .state = pool->state, .commandPool = pool->commandPool };
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
		commands->hasEpilogue = false;

		if (commands->timestampPool)
			vkResetQueryPool(pool->state->device, commands->timestampPool, 0, pool->state->timestampQueryCount);
		return commands;
	}

	void recordImageBarriers(VkCommandBuffer commandBuffer, const VkImageMemoryBarrier2* barriers, uint32_t numBarriers)
	{
		const VkDependencyInfo dependency{
			.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
			.imageMemoryBarrierCount = (uint32_t) numBarriers,
			.pImageMemoryBarriers = barriers,
		};
		vkCmdPipelineBarrier2(commandBuffer, &dependency);
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
			commands->hasEpilogue = true;
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
			recordImageBarriers(commandBuffer, barriers.data(), 1);
		}
		VkResult endCommandRecordingResult = vkEndCommandBuffer(commandBuffer);
		if (endCommandRecordingResult != VK_SUCCESS)
		{
			std::cout << "Failed to end command recording. Error code: " << endCommandRecordingResult << std::endl;
		}
	}

	TimelineSemaphore* createTimelineSemaphore(Device* device, uint64_t initialValue)
	{
		const VkSemaphoreTypeCreateInfo timelineInfo{
			.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
			.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE,
			.initialValue = initialValue,
		};
		const VkSemaphoreCreateInfo createInfo{
			.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
			.pNext = &timelineInfo,
		};
		TimelineSemaphore* result = new TimelineSemaphore{
			.state = device,
		};
		if (vkCreateSemaphore(device->device, &createInfo, nullptr, &result->semaphore) != VK_SUCCESS)
		{
			delete result;
			throw std::runtime_error("Failed to create semaphore!");
			return nullptr;
		}
		return result;
	}

	void waitTimeline(TimelinePoint point)
	{
		TimelineSemaphore* semaphore = point.semaphore;
		//assert((semaphore && semaphore->state && semaphore->semaphore) && "wait_timeline requires a live timeline semaphore");

		const VkSemaphore handle = semaphore->semaphore;
		const VkSemaphoreWaitInfo wait_info{
			.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO,
			.semaphoreCount = 1,
			.pSemaphores = &handle,
			.pValues = &point.value,
		};
		vkWaitSemaphores(
			semaphore->state->device,
			&wait_info,
			~uint64_t{ 0 });
	}

	void waitIdle(Device* device)
	{
		vkDeviceWaitIdle(device->device);
		device->drainContexts();
		device->nextPresentContext = 0;
	}

	PSO* createRasterPSO(Device* device, const ShaderStage& vertStage, const ShaderStage& fragStage, std::vector<ColorTargetDesc> colorTargets, Format depthFormat, Format stencilFormat,
		const RasterizationState& rastState, bool mesh, const ShaderStage& task = {})
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
			.cullMode = static_cast<VkCullModeFlags>(rastState.cull == CullMode::none ? VK_CULL_MODE_NONE : VK_CULL_MODE_BACK_BIT),
			.frontFace = VK_FRONT_FACE_CLOCKWISE,
			.depthBiasEnable = rastState.depthBiasConstant != 0.0f ||
						   rastState.depthBiasClamp != 0.0f ||
						   rastState.depthBiasSlope != 0.0f,
			.depthBiasConstantFactor = rastState.depthBiasConstant,
			.depthBiasClamp = rastState.depthBiasClamp,
			.depthBiasSlopeFactor = rastState.depthBiasSlope,
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
		for (size_t index = 0; index < colorTargets.size(); ++index)
		{
			const ColorTargetDesc& target = colorTargets.at(index);
			colorAttachments[index] = {
				.blendEnable = target.blend.enabled,
				.srcColorBlendFactor = toVk(target.blend.color.source),
				.dstColorBlendFactor = toVk(target.blend.color.destination),
				.colorBlendOp = static_cast<VkBlendOp>(target.blend.color.operation),
				.srcAlphaBlendFactor = toVk(target.blend.alpha.source),
				.dstAlphaBlendFactor = toVk(target.blend.alpha.destination),
				.alphaBlendOp = static_cast<VkBlendOp>(target.blend.alpha.operation),
				.colorWriteMask = target.writeMask,
			};
		}

		const VkPipelineColorBlendStateCreateInfo colorBlend{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
			.attachmentCount = (uint32_t) colorTargets.size(),
			.pAttachments = colorTargets.size() ? colorAttachments : nullptr,
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
		for (size_t index = 0; index < colorTargets.size(); ++index)
		{
			colorFormats[index] = toVk(colorTargets.at(index).format);
		}
		const VkPipelineRenderingCreateInfo renderingInfo{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
			.colorAttachmentCount = (uint32_t) colorTargets.size(),
			.pColorAttachmentFormats = colorTargets.size() ? colorFormats : nullptr,
			.depthAttachmentFormat = toVk(depthFormat),
			.stencilAttachmentFormat = toVk(stencilFormat),
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

		if (vkCreateGraphicsPipelines(device->device, VK_NULL_HANDLE, 1, &psoInfo, nullptr, &result->pso) != VK_SUCCESS)
		{
			delete result;
			return nullptr;
		}

		return result;
	}

	PSO* createGraphicsPSO(Device* device, GraphicsPSODesc graphicsDesc)
	{
		return createRasterPSO(device, graphicsDesc.vertex, graphicsDesc.fragment, graphicsDesc.colorTargets, graphicsDesc.depthFormat, graphicsDesc.stencilFormat, graphicsDesc.rasterization, false);
	}

	void bindPSO(CommandBuffer* commands, const PSO* pso)
	{
		vkCmdBindPipeline(commands->commandBuffer, pso->bindPoint, pso->pso);
	}

	std::vector<char> readShader(const char* fileName, const char* filePath)
	{
		std::ifstream file(fileName, std::ios::ate | std::ios::binary);
		if (!file.is_open())
		{
			throw std::runtime_error("failed to open file!");
		}
		std::vector<char> buffer(file.tellg());
		file.seekg(0, std::ios::beg);
		file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
		file.close();
		return buffer;
	}

	void destroyOwnedSwapchain(Device& device) noexcept
	{
		if (!device.swapchain)
			return;
		Swapchain* swapchain = device.swapchain;
		//assert(!swapchain->acquired && device.acquired_swapchain != swapchain);
		retireSwapchainHandle(*swapchain);
		swapchain->state = nullptr;
		delete swapchain;
		device.swapchain = nullptr;
	}

	void destroyPso(PSO* pso)
	{
		delete pso;
	}

	void destroyCommandPool(CommandPool* pool)
	{
		if (!pool) return;
		while (pool->first)
		{
			CommandBuffer* commands = pool->first;
			//assert(!commands->swapchain && "an acquired swapchain image must be presented before destroying its command pool");
			pool->first = commands->next;
			if (commands->timestampPool) vkDestroyQueryPool(pool->state->device, commands->timestampPool, nullptr);
			free(commands->timestampDestinations);
			free(commands->timestampResults);
			delete commands;
		}
		vkDestroyCommandPool(pool->state->device, pool->commandPool, nullptr);
		delete pool;
	}

	void destroyTimelineSemaphore(TimelineSemaphore* semaphore)
	{
		if (!semaphore) return;
		vkDestroySemaphore(semaphore->state->device, semaphore->semaphore, nullptr);
		delete semaphore;
	}

	void destroyDevice(Device* device)
	{
		delete device;
	}

}