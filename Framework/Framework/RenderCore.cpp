#include "RenderCore.hpp"


namespace gpu 
{

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

		void collect(VkDevice device, uint64_t completed_value) noexcept
		{
			while (count != 0 && entries[first].retireValue <= completed_value)
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

	// Device property variables
	// NOTE: Vectors may be better but at this point not really necessary
	const uint32_t maxDeviceExtensions = 512;
	const uint32_t maxInstanceExtensions = 512;
	constexpr uint32_t max_instance_layers = 64;
	constexpr uint32_t max_surface_formats = 64;
	const uint32_t maxPhysicalDevices = 32; // Really shouldn't be higher than 2 (in-built GPU, discreet GPU, maybe a second discreet GPU)
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
	
	struct RenderView
	{
		Device* state = nullptr;
		VkImageView view = VK_NULL_HANDLE;
		uint32_t width = 0;
		uint32_t height = 0;
		bool swapchain_view = false;
	};

	struct GpuRange
	{

	};

	struct GpuHeap
	{

	};

	struct RetiredSwapchain
	{
		VkSwapchainKHR handle = VK_NULL_HANDLE;
		VkImageView views[maxSwapchainImages]{};
		uint32_t viewCount = 0;
	};


	// TODO: in createDevice this needs to have all of its commands filled
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


	// Device, or the actual GPU we are interacting with
	struct Device
	{
		std::string deviceName = "";
		VkDevice device = VK_NULL_HANDLE;
		VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
		VkInstance vkInstance = VK_NULL_HANDLE;
		VkSurfaceKHR surface = VK_NULL_HANDLE;

		// TODO: Research why noapi doesnt define the .sType for these 2 variables under this comment
		VkPhysicalDeviceProperties deviceProps;
		VkPhysicalDeviceMemoryProperties deviceMemProps{};
		VkPhysicalDeviceDescriptorHeapPropertiesEXT deviceHeapProps{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_HEAP_PROPERTIES_EXT };
		DeviceCapabilities deviceCaps;

		Queue* queues = nullptr;
		uint32_t queueCount = 0;
		uint32_t queueFamilyCount = 0;
		uint32_t timestampQueryCount = 0;
		uint32_t queueFamilies[queueTypeCount]{};
		uint32_t textureMemoryType = VK_MAX_MEMORY_TYPES;
		uint64_t textureHeapAlignment = 16;
		DeviceCapabilities capabilities;

		Swapchain* swapChain = nullptr;
		Swapchain* acquiredSwapChain = nullptr;
		RetiredSwapchain retiredSwapchains[maxSwapchainImages]{};
		SwapchainDeleteQueue swapchainDeleteQueue;
		VkSemaphore presentationRetirement = VK_NULL_HANDLE;
		uint64_t presentationRetirementValue = 0;
		uint64_t completedPresentationRetirement = 0;
		uint64_t maxTimelineValueDifference = 0;

		bool texture_compression_etc2 = false;
		bool swapchain_color_space_enabled = false;

		PresentContext presentContexts[maxSwapchainImages]{};
		uint32_t presentContextCount = 0;
		uint32_t nextPresentContext = 0;

		DeviceFunctions fn;

		void pollPresentationRetirement();
		void waitPresentationRetirement(uint64_t value);
		uint64_t nextPresentationRetirement() noexcept;
		void waitPresentContext(PresentContext& context) noexcept;
		void drainContexts() noexcept;
		void finishPresentContext(PresentContext& context) noexcept;
		void queueRetiredSwapchain(RetiredSwapchain& retired) noexcept;
		void destroyPresentContext(PresentContext& context) noexcept;
		void pollPresentContexts() noexcept;

		VkResult createPresentContext(PresentContext& context);

		~Device();

		Device() = default;
		Device(const Device&) = delete;
		Device& operator=(const Device&) = delete;
	};

	// The queues that exist on the GPU that handle sending application info into the GPU
	struct Queue
	{
		VkQueue queue = VK_NULL_HANDLE;
		uint32_t familyIndex = 0;
		size_t commandSubmitCapacity = 0;
		size_t waitSubmitCapacity = 0;
		VkCommandBufferSubmitInfo* commandSubmitInfos = nullptr;
		VkSemaphoreSubmitInfo* waitSubmitInfos = nullptr;
		bool timestamps = false;
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

	VkFormat to_vk(Format format)
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

	template<typename T>
	T loadDeviceProc(VkDevice device, const char* name)
	{
		const PFN_vkVoidFunction proc = vkGetDeviceProcAddr(device, name);
		return reinterpret_cast<T>(proc);
	}

	struct Candidate
	{
		VkPhysicalDevice physical_device = VK_NULL_HANDLE;
		uint32_t queue_families[queueTypeCount]{};
		uint32_t queue_counts[queueTypeCount]{};
		bool timestamps[queueTypeCount]{};
		//uint32x3 copy_texture_granularity = { .x = 1, .y = 1, .z = 1 };
		VkPhysicalDeviceProperties properties{};
		VkPhysicalDeviceMemoryProperties memory_properties{};
		bool unified_image_layouts = false;
		bool address_commands = false;
		bool image_cube_array = false;
		bool texture_compression_bc = false;
		bool texture_compression_astc = false;
		bool texture_compression_etc2 = false;
		bool storage_input_output16 = false;
		bool khr_swapchain_maintenance1 = false;
		VkPhysicalDeviceDescriptorHeapPropertiesEXT heap_properties{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_HEAP_PROPERTIES_EXT };
		VkPhysicalDeviceVulkan12Properties vulkan12_properties{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_PROPERTIES };
		const char* missing_extensions[5]{};
		uint32_t missing_extension_count = 0;
	};
	
	constexpr VkMemoryPropertyFlags cpu_visible_memory_properties =
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT |
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
		VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

	constexpr VkMemoryPropertyFlags forbidden_memory_properties =
		VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT |
		VK_MEMORY_PROPERTY_PROTECTED_BIT |
		VK_MEMORY_PROPERTY_DEVICE_COHERENT_BIT_AMD |
		VK_MEMORY_PROPERTY_DEVICE_UNCACHED_BIT_AMD;

	bool is_usable_memory_type(const VkPhysicalDeviceMemoryProperties& properties, uint32_t index)
	{
		const VkMemoryType& type = properties.memoryTypes[index];
		if ((type.propertyFlags & forbidden_memory_properties) != 0)
			return false;
		return (properties.memoryHeaps[type.heapIndex].flags & VK_MEMORY_HEAP_TILE_MEMORY_BIT_QCOM) == 0;
	}

	// Setup methods

	VkResult enumerate_device_extensions(VkPhysicalDevice physical_device, VkExtensionProperties* values, uint32_t& count) noexcept
	{
		count = static_cast<uint32_t>(maxDeviceExtensions);
		const VkResult result = vkEnumerateDeviceExtensionProperties(physical_device, nullptr, &count, values);
		if (result != VK_SUCCESS)
		{
			return VK_INCOMPLETE;
		}
		else
		{
			return VK_SUCCESS;
		}
	}

	VkResult enumerate_instance_extensions(VkExtensionProperties* values, uint32_t& count, uint32_t arraySize) noexcept
	{
		count = static_cast<uint32_t>(arraySize);
		const VkResult result = vkEnumerateInstanceExtensionProperties(nullptr, &count, values);
		return result;
	}

	bool has_name(const VkExtensionProperties* values, const char* name, uint32_t arraySize)
	{
		for (size_t index = 0; index < arraySize; ++index)
		{
			if (strcmp(values[index].extensionName, name) == 0)
				return true;
		}
		return false;
	}

	bool has_name(const VkLayerProperties* values, const char* name, uint32_t arraySize)
	{
		for (size_t index = 0; index < arraySize; ++index)
		{
			if (strcmp(values[index].layerName, name) == 0)
				return true;
		}
		return false;
	}

	VkResult inspect_candidate(VkPhysicalDevice physical_device, VkSurfaceKHR surface, bool khr_surface_maintenance1, bool ext_surface_maintenance1,
		const DesiredDevice& desc, Candidate& output) noexcept
	{
		VkExtensionProperties extensions[maxDeviceExtensions]{};
		uint32_t extension_count = 0;
		const VkResult extension_error = enumerate_device_extensions(physical_device, extensions, extension_count);
		if (extension_error != VK_SUCCESS)
			return VK_INCOMPLETE;
		const bool unified_image_layouts_extension = has_name(
			extensions,
			VK_KHR_UNIFIED_IMAGE_LAYOUTS_EXTENSION_NAME, extension_count);
		constexpr const char* required_extensions[]{
			VK_EXT_DESCRIPTOR_HEAP_EXTENSION_NAME,
			VK_KHR_SHADER_UNTYPED_POINTERS_EXTENSION_NAME,
			VK_EXT_MESH_SHADER_EXTENSION_NAME,
			VK_KHR_SWAPCHAIN_EXTENSION_NAME,
		};
		for (uint32_t index = 0; index < (surface ? 4u : 3u); ++index)
		{
			if (!has_name( extensions, required_extensions[index], extension_count))
				output.missing_extensions[output.missing_extension_count++] = required_extensions[index];
		}
		const bool khr_swapchain_maintenance1 = khr_surface_maintenance1 &&
			has_name( extensions, VK_KHR_SWAPCHAIN_MAINTENANCE_1_EXTENSION_NAME, extension_count);
		const bool ext_swapchain_maintenance1 = ext_surface_maintenance1 &&
			has_name(extensions, VK_EXT_SWAPCHAIN_MAINTENANCE_1_EXTENSION_NAME, extension_count);
		if (surface && !khr_swapchain_maintenance1 && !ext_swapchain_maintenance1)
		{
			output.missing_extensions[output.missing_extension_count++] = khr_surface_maintenance1 && ext_surface_maintenance1
				? "VK_KHR_swapchain_maintenance1 or VK_EXT_swapchain_maintenance1"
				: khr_surface_maintenance1 ? VK_KHR_SWAPCHAIN_MAINTENANCE_1_EXTENSION_NAME : VK_EXT_SWAPCHAIN_MAINTENANCE_1_EXTENSION_NAME;
		}
		if (output.missing_extension_count) return VK_INCOMPLETE;

		Candidate result{
			.physical_device = physical_device,
		};
		result.heap_properties.pNext = &result.vulkan12_properties;
		VkPhysicalDeviceProperties2 properties2{
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
			.pNext = &result.heap_properties,
		};
		vkGetPhysicalDeviceProperties2(physical_device, &properties2);
		result.properties = properties2.properties;
		result.heap_properties.pNext = nullptr;
		result.vulkan12_properties.pNext = nullptr;
		if (result.properties.apiVersion < VK_API_VERSION_1_4)
			return VK_INCOMPLETE;

		vkGetPhysicalDeviceMemoryProperties(physical_device, &result.memory_properties);
		bool cpu_visible_memory = false;
		for (uint32_t index = 0; index < result.memory_properties.memoryTypeCount; ++index)
		{
			if (is_usable_memory_type(result.memory_properties, index) &&
				(result.memory_properties.memoryTypes[index].propertyFlags & cpu_visible_memory_properties) == cpu_visible_memory_properties)
			{
				cpu_visible_memory = true;
				break;
			}
		}
		if (!cpu_visible_memory) return VK_INCOMPLETE;

		const bool address_commands_extension = has_name( extensions, VK_KHR_DEVICE_ADDRESS_COMMANDS_EXTENSION_NAME, extension_count);
		DesiredFeatures features(surface != VK_NULL_HANDLE, unified_image_layouts_extension, address_commands_extension);
		vkGetPhysicalDeviceFeatures2(physical_device, &features.core);
		const bool required_features =
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
		if (!required_features)
			return VK_INCOMPLETE;
		result.unified_image_layouts = unified_image_layouts_extension && features.unifiedImageLayouts.unifiedImageLayouts == VK_TRUE;
//#if !defined(NOGRAPHICSAPI_TEST_BUFFER_COMMANDS)
		result.address_commands = address_commands_extension && features.addressCommands.deviceAddressCommands == VK_TRUE;
//#endif
		result.image_cube_array = features.core.features.imageCubeArray == VK_TRUE;
		result.texture_compression_bc = features.core.features.textureCompressionBC == VK_TRUE;
		result.texture_compression_astc = features.core.features.textureCompressionASTC_LDR == VK_TRUE;
		result.texture_compression_etc2 = features.core.features.textureCompressionETC2 == VK_TRUE;
		result.storage_input_output16 = features.vulkan11.storageInputOutput16 == VK_TRUE;
		result.khr_swapchain_maintenance1 = khr_swapchain_maintenance1;

		uint32_t available_queue_count = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &available_queue_count, nullptr);
		if (available_queue_count > maxQueueFamilies)
			return VK_INCOMPLETE;
		VkQueueFamilyProperties queues[maxQueueFamilies]{};
		uint32_t queue_count = available_queue_count;
		vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_count, queues);
		for (uint32_t index = 0; index < queue_count; ++index)
		{
			if (queues[index].queueCount == 0)
				continue;
			const VkQueueFlags flags = queues[index].queueFlags;
			uint32_t type = 0;
			if (flags & VK_QUEUE_GRAPHICS_BIT)
			{
				if (!(flags & VK_QUEUE_COMPUTE_BIT)) continue;
			}
			else if (flags & VK_QUEUE_COMPUTE_BIT)
			{
				type = 1;
			}
			else if ((flags & VK_QUEUE_TRANSFER_BIT) &&
				!(flags & ~(VK_QUEUE_TRANSFER_BIT | VK_QUEUE_SPARSE_BINDING_BIT | VK_QUEUE_PROTECTED_BIT)))
			{
				type = 2;
			}
			else
			{
				continue;
			}
			if (result.queue_counts[type] != 0) continue;
			VkBool32 presentation_supported = VK_TRUE;
			if (surface && type == 0)
			{
				const VkResult presentation_error = vkGetPhysicalDeviceSurfaceSupportKHR(physical_device, index, surface, &presentation_supported);
				if (presentation_error != VK_SUCCESS)
					return VK_INCOMPLETE;
			}
			if (presentation_supported == VK_TRUE)
			{
				result.queue_families[type] = index;
				result.queue_counts[type] = queues[index].queueCount;
				result.timestamps[type] = desc.timestamp_query_count != 0 && queues[index].timestampValidBits == 64 && features.vulkan12.hostQueryReset;
				if (type == 2)
				{
					//result.copy_texture_granularity = {
					//	.x = queues[index].minImageTransferGranularity.width,
					//	.y = queues[index].minImageTransferGranularity.height,
					//	.z = queues[index].minImageTransferGranularity.depth,
					//};
				}
			}
		}
		if (result.queue_counts[0] == 0 || (desc.desiredComputeQueues && result.queue_counts[1] == 0) ||
			(desc.desiredTransferQueues && result.queue_counts[2] == 0))
			return VK_INCOMPLETE;

		output = result;
		return VK_SUCCESS;
	}

	VkResult enumerate_physical_devices(VkInstance instance, VkPhysicalDevice* values, uint32_t& count, uint32_t deviceSize) noexcept
	{
		count = static_cast<uint32_t>(deviceSize);
		const VkResult result = vkEnumeratePhysicalDevices(instance, &count, values);
		if (result != VK_SUCCESS)
		{
			return result;
		}
		else
		{
			return VK_SUCCESS;
		}
	}

#if !defined(NDEBUG)
	VkResult enumerate_instance_layers(VkLayerProperties* values, uint32_t& count, uint32_t arraySize) noexcept
	{
		count = static_cast<uint32_t>(arraySize);
		const VkResult result = vkEnumerateInstanceLayerProperties(&count, values);
		return result;
	}
#endif

	//DeviceInit fail_device_creation(Device* device, Error error, const char* message = nullptr) noexcept
	//{
	//	if (error_callback && (message || error == Error::unsupported))
	//		error_callback(message ? message : "Required Vulkan device, memory, or display capabilities are unavailable.", false);
	//	delete device;
	//	return {
	//		.error = error,
	//	};
	//}

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
		std::vector<VkQueueFamilyProperties> queueFamilyProperties;
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
			std::cout << "This device lacked the required features" << deviceProperties2.properties.deviceName << std::endl;
			return 0;
		}

		// Determine queue count and families, check that needed queues exist
		bool presentQueueFound = false;
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queueCount, nullptr);
		queueFamilyProperties.resize(queueCount);
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queueCount, queueFamilyProperties.data());
		for (uint32_t index = 0; index < queueCount; ++index)
		{
			if (queueFamilyProperties.at(index).queueCount == 0)
				continue;

			const VkFlags queueFlag = queueFamilyProperties.at(index).queueFlags;
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

		//if (!presentQueueFound)
		//{
		//	return 0;
		//}

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

		// For some reason it is required to enumerate once, resize vector, enumerate again.
		vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
		devices.resize(deviceCount);
		VkResult physicalDeviceResult = vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());
		

		if (physicalDeviceResult != VK_SUCCESS)
		{
			throw std::runtime_error("No GPU found in system!");
		}
		
		// Iterate each device and run check
		for (const auto& device : devices)
		{
			//uint32_t currentScore = scoreDevice(device, deviceInfo->surface);
			uint32_t currentScore = 0;
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
	DeviceInit createDevice(const DesiredDevice& desiredDevice, GLFWwindow* window)
	{
		Device* device = new Device;

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
		

		// TODO: enable instance extensions here ALSO GET GLFW EXTENSIONS
		uint32_t glfwCount = 0;
		auto glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwCount);
		if (presentation)
		{
			enabledInstanceExtensions.push_back(VK_EXT_SWAPCHAIN_COLOR_SPACE_EXTENSION_NAME);
			enabledInstanceExtensions.push_back(VK_KHR_SURFACE_EXTENSION_NAME);
			enabledInstanceExtensions.push_back(VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME);
			enabledInstanceExtensions.push_back(VK_KHR_SURFACE_MAINTENANCE_1_EXTENSION_NAME);
			enabledInstanceExtensions.push_back(VK_EXT_SURFACE_MAINTENANCE_1_EXTENSION_NAME);
		}
		// Assign GLFW extensions
		for (int index = 0; index < extensionCount; index++)
		{
			enabledInstanceExtensions.push_back(extensions.data()->extensionName);
		}
		for (int index = 0; index < glfwCount; ++index)
		{
			enabledInstanceExtensions.push_back(glfwExtensions[index]);
		}


		// Enable validation layer if debugging
		if (enableValidationLayers)
		{
			enabledLayers.push_back("VK_LAYER_KHRONOS_validation");
		}

		const VkInstanceCreateInfo instanceInfo
		{
			.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
			.pApplicationInfo = &appInfo,
			.enabledLayerCount = (uint32_t) enabledLayers.size(),
			.ppEnabledLayerNames = enabledLayers.data(),
			.enabledExtensionCount = glfwCount,
			.ppEnabledExtensionNames = glfwExtensions
		};
		VkResult instanceResult = vkCreateInstance(&instanceInfo, nullptr, &device->vkInstance);

		if (instanceResult != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create VK Instance!");
		}

		// Find physical device to use and begin populating the rest of the Device* device param
		// TODO: Get physicalDevice Heap properties
		pickPhysicalDevice(device->vkInstance, device);
		vkGetPhysicalDeviceProperties(device->physicalDevice, &device->deviceProps);
		vkGetPhysicalDeviceMemoryProperties(device->physicalDevice, &device->deviceMemProps);

		// Create surface for GLFW
		VkResult surfaceResult = glfwCreateWindowSurface(device->vkInstance, window, nullptr, &device->surface);
		if (surfaceResult != VK_SUCCESS)
		{
			throw std::runtime_error("Failure to create window surface");
		}
		

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

		device->fn.cmdPushData = loadDeviceProc<PFN_vkCmdPushDataEXT>(device->device, "vkCmdPushDataEXT");

		// Queue selection
		uint32_t queueCount = 0;

		vkGetPhysicalDeviceQueueFamilyProperties(device->physicalDevice, &queueCount, nullptr);
		std::vector<VkQueueFamilyProperties> queueFamilyProperties(queueCount); // Gets each queue family, not each queue important detail
		vkGetPhysicalDeviceQueueFamilyProperties(device->physicalDevice, &queueCount, queueFamilyProperties.data());
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
			queueCounts[queueType] = requestedQueueCounts[queueType] < queueFamilyProperties.at(queueType).queueCount ? requestedQueueCounts[queueType] : queueFamilyProperties.at(queueType).queueCount;
			device->queueCount += queueCounts[queueType];
		}

		// Find queue family indexes and those that supports presentation
		// Similar to how we check for presentation support in scoreDevice
		for (int index = 0; index < queueCount; ++index)
		{
			uint32_t queueType = 0;
			const VkQueueFlags flags = queueFamilyProperties.at(index).queueFlags;
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
			//if (queueFamilyProperties.at(index).queueFamilyProperties.queueFlags != 0) continue;
			// organize each queue of that type with its index, check that our graphics/present queue family(commonly family 0) supports presentation
			if (device->surface && queueType == 0) // Since family 0 is the one that contains present Queue, we check this mostly
			{
				// TODO: PRESENT SUPPORT RETURNING FALSE
				vkGetPhysicalDeviceSurfaceSupportKHR(device->physicalDevice, index, device->surface, &presentSupport);
				if (presentSupport != VK_SUCCESS)
				{
					std::cout << "Family index: " << index << std::endl;
					throw std::runtime_error("Surface/queue index does not support presentation");
				}
				
			}
			if (presentSupport == VK_SUCCESS)
			{
				queueFamilyIndex[queueType] = index;
				//desiredTimestamps[queueType] = desiredDevice.timeStampQueryCount != 0 && queueFamilyProperties.at(index).queueFamilyProperties.timestampValidBits == 64 && enabledFeatures.vulkan12.hostQueryReset;
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
			// TODO Out of range error?
			//device->queueFamilies[device->queueFamilyCount] = queueFamilyProperties.at(queueType); // TODO: this is wrong, I think
			device->queueFamilyCount++;
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
			.enabledExtensionCount = (uint32_t) enableDeviceExentsions.size(),
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


		return
		{
			.device = device,
		};
	}

	DeviceInit createDevice2(const DesiredDevice& desiredDevice, GLFWwindow* window)
	{
		Device* device = new Device;

		// Get extensions
		const bool presentation = true;
		uint32_t extensionCount = 0;
		std::vector<VkExtensionProperties> extensions;
		std::vector<const char*> enabledInstanceExtensions;
		std::vector<const char*> enabledLayers;
		uint32_t layerCount = 0;
		vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data());
		extensions.resize(extensionCount);
		vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data());

		// Create context and instance of VK
		const VkApplicationInfo appInfo
		{
			.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
			.pApplicationName = "Framework",
			.applicationVersion = VK_MAKE_API_VERSION(1, 0, 0, 0),
			.pEngineName = "N/A",
			.engineVersion = VK_MAKE_API_VERSION(1, 0, 0, 0),
			.apiVersion = VK_API_VERSION_1_4
		};


		// TODO: enable instance extensions here ALSO GET GLFW EXTENSIONS
		uint32_t glfwCount = 0;
		auto glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwCount);
		if (presentation)
		{
			//enabledInstanceExtensions.push_back(VK_EXT_SWAPCHAIN_COLOR_SPACE_EXTENSION_NAME);
			//enabledInstanceExtensions.push_back(VK_KHR_SURFACE_EXTENSION_NAME);
			//enabledInstanceExtensions.push_back(VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME);
			//enabledInstanceExtensions.push_back(VK_KHR_SURFACE_MAINTENANCE_1_EXTENSION_NAME);
			//enabledInstanceExtensions.push_back(VK_EXT_SURFACE_MAINTENANCE_1_EXTENSION_NAME);
		}
		// Assign GLFW extensions
		for (int index = 0; index < extensionCount; index++)
		{
			enabledInstanceExtensions.push_back(extensions.at(index).extensionName);
		}
		//for (int index = 0; index < glfwCount; ++index)
		//{
		//	enabledInstanceExtensions.push_back(glfwExtensions[index]);
		//}


		// Enable validation layer if debugging
		if (enableValidationLayers)
		{
			enabledLayers.push_back("VK_LAYER_KHRONOS_validation");
		}

		const VkInstanceCreateInfo instanceInfo
		{
			.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
			.pApplicationInfo = &appInfo,
			.enabledLayerCount = (uint32_t)enabledLayers.size(),
			.ppEnabledLayerNames = enabledLayers.data(),
			.enabledExtensionCount = (uint32_t)enabledInstanceExtensions.size(),
			.ppEnabledExtensionNames = enabledInstanceExtensions.data(),
		};
		VkResult instanceResult = vkCreateInstance(&instanceInfo, nullptr, &device->vkInstance);

		if (instanceResult != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create VK Instance!");
		}

		// Find physical device to use and begin populating the rest of the Device* device params
		pickPhysicalDevice(device->vkInstance, device);


		// Enable features of device
		DesiredFeatures enabled_features(presentation, true, true);
		//device->textureCompressionEtc2 = selected.texture_compression_etc2;
		enabled_features.core.features.imageCubeArray = VK_TRUE;
		enabled_features.core.features.samplerAnisotropy = VK_TRUE;
		enabled_features.core.features.shaderInt16 = VK_TRUE;
		enabled_features.core.features.depthBiasClamp = VK_TRUE;
		enabled_features.core.features.independentBlend = VK_TRUE;
		enabled_features.core.features.textureCompressionBC = VK_TRUE;
		enabled_features.core.features.textureCompressionASTC_LDR = VK_TRUE;
		enabled_features.core.features.textureCompressionETC2 = VK_TRUE;
		enabled_features.core.features.fragmentStoresAndAtomics = VK_TRUE;
		enabled_features.core.features.vertexPipelineStoresAndAtomics = VK_TRUE;
		enabled_features.core.features.shaderStorageImageReadWithoutFormat = VK_TRUE;
		enabled_features.core.features.shaderStorageImageWriteWithoutFormat = VK_TRUE;
		enabled_features.core.features.multiDrawIndirect = VK_TRUE;
		enabled_features.core.features.drawIndirectFirstInstance = VK_TRUE;
		enabled_features.vulkan11.storageBuffer16BitAccess = VK_TRUE;
		enabled_features.vulkan11.storagePushConstant16 = VK_TRUE;
		enabled_features.vulkan11.storageInputOutput16 = VK_TRUE;
		enabled_features.vulkan11.shaderDrawParameters = VK_TRUE;
		enabled_features.vulkan12.shaderFloat16 = VK_TRUE;
		enabled_features.vulkan12.scalarBlockLayout = VK_TRUE;
		enabled_features.vulkan12.timelineSemaphore = VK_TRUE;
		enabled_features.vulkan12.hostQueryReset = VK_TRUE;
		enabled_features.vulkan12.bufferDeviceAddress = VK_TRUE;
		enabled_features.vulkan13.synchronization2 = VK_TRUE;
		enabled_features.vulkan13.dynamicRendering = VK_TRUE;
		enabled_features.vulkan13.maintenance4 = VK_TRUE;
		enabled_features.vulkan14.maintenance5 = VK_TRUE;
		enabled_features.descriptorHeap.descriptorHeap = VK_TRUE;
		enabled_features.addressCommands.deviceAddressCommands = VK_TRUE;
		enabled_features.untypedPointers.shaderUntypedPointers = VK_TRUE;
		enabled_features.unifiedImageLayouts.unifiedImageLayouts = VK_TRUE;
		enabled_features.meshShader.taskShader = VK_TRUE;
		enabled_features.meshShader.meshShader = VK_TRUE;
		enabled_features.swapchainMaintenance1.swapchainMaintenance1 = VK_TRUE;


		/// Queue creation section
		uint32_t queueCount = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(device->physicalDevice, &queueCount, nullptr);
		std::vector<VkQueueFamilyProperties> queues(queueCount); // Gets each queue family, not each queue important detail
		vkGetPhysicalDeviceQueueFamilyProperties(device->physicalDevice, &queueCount, queues.data());
		// For each queue type, iterate and get desired queue amount, and assign it to device queue struct
		const uint32_t requestedQueueCounts[]{ desiredDevice.desiredGraphicsQueues, desiredDevice.desiredComputeQueues, desiredDevice.desiredTransferQueues };
		uint32_t queueCountsOfEachType[queueTypeCount];
		uint32_t queueFamilyIndex[maxQueueFamilies];
		bool desiredTimestamps[queueTypeCount];
		bool timestamps[queueTypeCount]{};
		for (uint32_t queueType = 0; queueType < queueCount; ++queueType)
		{
			// TODO: Possibly incorrect logic, debug for sure
			// NOTE: assign queue counts from requestedQueues[] if lower than the queues in the family we are looking at, otherwise assign queue counts from family if requestedQueues[] is larger
			// NOTE: noapi uses the queue_counts found in their own inspect_candidate. This should be roughly the same though as that also finds ALL of the queues available
			queueCountsOfEachType[queueType] = requestedQueueCounts[queueType] < queues.at(queueType).queueCount ? requestedQueueCounts[queueType] : queues.at(queueType).queueCount;
			device->queueCount += queueCountsOfEachType[queueType];
		}

		// Find index of queue families
		for (uint32_t index = 0; index < queueCount; ++index)
		{
			if (queues[index].queueCount == 0)
				continue;
			const VkQueueFlags flags = queues[index].queueFlags;
			uint32_t type = 0;
			if (flags & VK_QUEUE_GRAPHICS_BIT)
			{
				if (!(flags & VK_QUEUE_COMPUTE_BIT)) continue;
			}
			else if (flags & VK_QUEUE_COMPUTE_BIT)
			{
				type = 1;
			}
			else if ((flags & VK_QUEUE_TRANSFER_BIT) &&
				!(flags & ~(VK_QUEUE_TRANSFER_BIT | VK_QUEUE_SPARSE_BINDING_BIT | VK_QUEUE_PROTECTED_BIT)))
			{
				type = 2;
			}
			else
			{
				continue;
			}
			if (queueCountsOfEachType[type] != 0) continue;
			VkBool32 presentation_supported = VK_TRUE;
			if (device->surface && type == 0)
			{
				VkResult presentResult = vkGetPhysicalDeviceSurfaceSupportKHR(device->physicalDevice, index, device->surface, &presentation_supported);
				if (presentResult != VK_SUCCESS)
					throw std::runtime_error("Surface does not support present");
			}
			if (presentation_supported == VK_TRUE)
			{
				device->queueFamilies[type] = index;
				queueCountsOfEachType[type] = queues[index].queueCount;
				timestamps[type] = desiredDevice.timestamp_query_count != 0 && queues[index].timestampValidBits == 64 && enabled_features.vulkan12.hostQueryReset;
				if (type == 2)
				{
					// Transfer queue stuff
				//	result.copy_texture_granularity = {
				//		.x = queues[index].minImageTransferGranularity.width,
				//		.y = queues[index].minImageTransferGranularity.height,
				//		.z = queues[index].minImageTransferGranularity.depth,
				//	};
				//}
				}

			}
		}

		//enabled_features.vulkan12.hostQueryReset = timestamps[0] || timestamps[1] || timestamps[2];


		// Create empty queue array for how many queues we have deciced to use
		device->queues = new Queue[device->queueCount];
		float* queuePriorities = new float[device->queueCount];
		for (uint32_t index = 0; index < device->queueCount; ++index)
			queuePriorities[index] = 1.0f;
		VkDeviceQueueCreateInfo queueInfo[queueTypeCount]{};
		uint32_t firstQueue = 0;
		for (uint32_t type = 0; type < queueTypeCount; ++type)
		{
			if (queueCountsOfEachType[type] == 0) continue;
			//device->queueFamilies[device->queueFamilyCount] = queues.at(type); // Needs to be index of that family, done in larger loop before this
			queueInfo[device->queueFamilyCount] =
			{
				.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
				.queueFamilyIndex = device->queueFamilies[type],
				.queueCount = queueCountsOfEachType[type],
				.pQueuePriorities = queuePriorities + firstQueue,
			};
			firstQueue += queueCountsOfEachType[type];
		}


		// Create device
		uint32_t deviceExtensionCount = 7;
		const char* enabledDeviceExtensions[7]{};
		//enabledDeviceExtensions.reserve(7);

		enabledDeviceExtensions[0] = VK_EXT_DESCRIPTOR_HEAP_EXTENSION_NAME;
		enabledDeviceExtensions[1] = VK_KHR_DEVICE_ADDRESS_COMMANDS_EXTENSION_NAME;
		enabledDeviceExtensions[2] = VK_KHR_SHADER_UNTYPED_POINTERS_EXTENSION_NAME;
		enabledDeviceExtensions[3] = VK_KHR_UNIFIED_IMAGE_LAYOUTS_EXTENSION_NAME;
		enabledDeviceExtensions[4] = VK_EXT_MESH_SHADER_EXTENSION_NAME;
		enabledDeviceExtensions[5] = VK_KHR_SWAPCHAIN_EXTENSION_NAME;
		enabledDeviceExtensions[6] = VK_KHR_SWAPCHAIN_MAINTENANCE_1_EXTENSION_NAME;

		const VkDeviceCreateInfo deviceInfo{
			.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
			.pNext = &enabled_features.core,
			.queueCreateInfoCount = device->queueFamilyCount, // passing as 0?!?!?
			.pQueueCreateInfos = queueInfo,
			.enabledExtensionCount = deviceExtensionCount,
			.ppEnabledExtensionNames = enabledDeviceExtensions, // broken, not being passed correctly?
		};
		VkResult deviceResult = vkCreateDevice(device->physicalDevice, &deviceInfo, nullptr, &device->device);
		if (deviceResult != VK_SUCCESS)
		{
			throw std::runtime_error("No device created!");
		}
		delete[] queuePriorities;

		// Get queues from device
		firstQueue = 0;
		for (uint32_t type = 0; type < queueTypeCount; ++type)
		{
			for (uint32_t index = 0; index < queueCountsOfEachType[type]; ++index)
			{
				Queue& queue = device->queues[firstQueue++];
				queue.familyIndex = device->queueFamilies[type];
				queue.timestamps = timestamps[type];
				vkGetDeviceQueue(device->device, queue.familyIndex, index, &queue.queue);
			}
		}
		device->fn.cmdPushData = loadDeviceProc<PFN_vkCmdPushDataEXT>(device->device, "vkCmdPushDataEXT"); // assigning currently used device command

		const VkSemaphoreTypeCreateInfo type_info{
			.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
			.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE,
		};
		const VkSemaphoreCreateInfo semaphore_info{
			.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
			.pNext = &type_info,
		};
		VkResult semaphoreResult = vkCreateSemaphore(device->device, &semaphore_info, nullptr, &device->presentationRetirement);
		if (semaphoreResult != VK_SUCCESS)
			throw std::runtime_error("Failed to recreate Semaphore");
		for (uint32_t index = 0; index < device->presentContextCount; ++index)
		{
			device->createPresentContext(device->presentContexts[index]);
			//s
		}

		// Assign device caps
		device->capabilities = {
			.deviceName = "ADD DEVICE NAME",
			.queueCount = device->queueCount,
			.queueGeneralCount = queueCountsOfEachType[0],
			.queueComputeCount = queueCountsOfEachType[1],
			.queueTransferCount = queueCountsOfEachType[2],
			//.copy_texture_granularity = selected.copy_texture_granularity,
			//.max_push_data_size = state->heap_properties.maxPushDataSize,
			//.texture_heap_alignment = state->texture_heap_alignment,
			//.timestamp_period_ns = selected.properties.limits.timestampPeriod,
			//.sub_texel_precision_bits = selected.properties.limits.subTexelPrecisionBits,
			.textureCompressionBC = VK_TRUE,
			.textureCompressionASTC = VK_TRUE,
			.storageInputOutput16 = VK_TRUE,
			.indirectMeshDraw = true,
		};

		device->swapChain = new Swapchain;
		device->swapChain->state = device;
		//device->swapChain->format = desc.swapchain_format;
		device->swapChain->colorSpace = VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT;
		VkResult swapchainError = recreateSwapchain(*device->swapChain);
		if (swapchainError != VK_SUCCESS)
			throw std::runtime_error("Failed to create swapchain");


		return
		{
			.device = device,
		};
	}

	// noapi create device since mine is FRIED
	DeviceInit create_device(const DesiredDevice& desc)
	{
//		assert(desc.desired_queue_count != 0 && "create_device requires at least one queue");
		const bool presentation = true;
//		assert(desc.desired_swapchain_image_count != 0 && desc.desired_swapchain_image_count <= max_swapchain_images &&
//			"swapchain image count must fit the wrapper's presentation context array");
//#if !defined(_WIN32)
//		if (presentation)
//			return { .error = Error::unsupported };
//#endif

		uint32_t loader_version = VK_API_VERSION_1_0;
		VkResult error = vkEnumerateInstanceVersion(&loader_version);
		if (error != VK_SUCCESS)
			return { .error = error };
		//if (loader_version < VK_API_VERSION_1_4)
		//{
		//	if (error_callback) error_callback("Vulkan 1.4 or newer is required. Update the graphics driver.", false);
		//	return { .error = Error::unsupported };
		//}

		uint32_t glfwExtCount = 0;
		auto glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtCount);

		Device* state = new Device;
		state->timestampQueryCount = desc.timestamp_query_count;
		state->presentContextCount = presentation ? desc.desiredSwapchainImageCount : 0;
		VkExtensionProperties instance_extensions[maxInstanceExtensions]{};
		uint32_t instance_extension_count = 0;
		error = enumerate_instance_extensions( instance_extensions, instance_extension_count, maxInstanceExtensions);
		if (error != VK_SUCCESS)
			return { .error = error };
//#if defined(_WIN32)
		state->swapchain_color_space_enabled = presentation &&
			has_name( instance_extensions, VK_EXT_SWAPCHAIN_COLOR_SPACE_EXTENSION_NAME, instance_extension_count);
		const bool khr_surface_maintenance1 = presentation && has_name(
			 instance_extensions,
			VK_KHR_SURFACE_MAINTENANCE_1_EXTENSION_NAME, instance_extension_count);
		const bool ext_surface_maintenance1 = presentation && has_name(
			 instance_extensions,
			VK_EXT_SURFACE_MAINTENANCE_1_EXTENSION_NAME, instance_extension_count);
		if (presentation)
		{
			const char* missing_extensions[5]{};
			uint32_t missing_extension_count = 0;
			constexpr const char* required_extensions[]{
				VK_KHR_SURFACE_EXTENSION_NAME,
				//VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
				VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME,
			};
			for (const char* name : required_extensions)
			{
				if (!has_name(instance_extensions, name, instance_extension_count))
					missing_extensions[missing_extension_count++] = name;
			}
			//if (!khr_surface_maintenance1 && !ext_surface_maintenance1)
			//	missing_extensions[missing_extension_count++] = "VK_KHR_surface_maintenance1 or VK_EXT_surface_maintenance1";
			//if (desc.swapchain_color_space != ColorSpace::srgb && !state->swapchain_color_space_enabled)
			//	missing_extensions[missing_extension_count++] = VK_EXT_SWAPCHAIN_COLOR_SPACE_EXTENSION_NAME;
			if (missing_extension_count)
			{
				char message[1024] = "Cannot initialize Vulkan presentation.";
				//append_capability_diagnostic(message, "Vulkan instance", { missing_extensions, missing_extension_count });
				return { .error = VK_INCOMPLETE };
			}
		}
//#else
		//constexpr bool khr_surface_maintenance1 = false;
		//constexpr bool ext_surface_maintenance1 = false;
//#endif
#if !defined(NDEBUG)
		VkLayerProperties layers[max_instance_layers]{};
		uint32_t layer_count = 0;
		error = enumerate_instance_layers({ layers }, layer_count, max_instance_layers);
		if (error != VK_SUCCESS)
			return { .error = error };
		const bool debug_utils_available = has_name( instance_extensions, VK_EXT_DEBUG_UTILS_EXTENSION_NAME, instance_extension_count);
		const bool validation_available = has_name( layers, "VK_LAYER_KHRONOS_validation", layer_count);
#endif

		const char* enabled_instance_extensions[7]{};
		uint32_t enabled_instance_extension_count = 0;
		const char* enabled_layers[1]{};
		uint32_t enabled_layer_count = 0;
#if !defined(NDEBUG)
		if (debug_utils_available) enabled_instance_extensions[enabled_instance_extension_count++] = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
		if (validation_available) enabled_layers[enabled_layer_count++] = "VK_LAYER_KHRONOS_validation";
#endif
//#if defined(_WIN32)
		if (presentation)
		{
			if (state->swapchain_color_space_enabled)
				enabled_instance_extensions[enabled_instance_extension_count++] = VK_EXT_SWAPCHAIN_COLOR_SPACE_EXTENSION_NAME;
			enabled_instance_extensions[enabled_instance_extension_count++] = VK_KHR_SURFACE_EXTENSION_NAME;
			//enabled_instance_extensions[enabled_instance_extension_count++] = VK_KHR_WIN32_SURFACE_EXTENSION_NAME;
			enabled_instance_extensions[enabled_instance_extension_count++] = VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME;
			if (khr_surface_maintenance1)
				enabled_instance_extensions[enabled_instance_extension_count++] = VK_KHR_SURFACE_MAINTENANCE_1_EXTENSION_NAME;
			if (ext_surface_maintenance1)
				enabled_instance_extensions[enabled_instance_extension_count++] = VK_EXT_SURFACE_MAINTENANCE_1_EXTENSION_NAME;
		}
//#endif

		// Add GLFW extensions
		for (uint32_t index = 0; index < glfwExtCount; ++index)
		{
			enabled_instance_extensions[enabled_instance_extension_count++] = glfwExtensions[index];
		}

		const VkApplicationInfo app_info{
			.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
			.pApplicationName = "NoGraphicsAPI application",
			.applicationVersion = VK_MAKE_API_VERSION(0, 0, 1, 0),
			.pEngineName = "NoGraphicsAPI",
			.engineVersion = VK_MAKE_API_VERSION(0, 0, 1, 0),
			.apiVersion = VK_API_VERSION_1_4,
		};
		const VkInstanceCreateInfo instance_info{
			.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
			.pApplicationInfo = &app_info,
			.enabledLayerCount = enabled_layer_count,
			.ppEnabledLayerNames = enabled_layer_count ? enabled_layers : nullptr,
			.enabledExtensionCount = enabled_instance_extension_count,
			.ppEnabledExtensionNames = enabled_instance_extension_count ? enabled_instance_extensions : nullptr,
		};
		error = vkCreateInstance(&instance_info, nullptr, &state->vkInstance);
		if (error != VK_SUCCESS)
			return { .error = error };

		

		VkResult surfaceResult = glfwCreateWindowSurface(state->vkInstance, desc.window, nullptr, &state->surface);
		if (surfaceResult != VK_SUCCESS)
		{
			throw std::runtime_error("Failure to create window surface");
		}

//#if !defined(NDEBUG)
//		if (debug_utils_available)
//		{
//			const PFN_vkCreateDebugUtilsMessengerEXT create_debug =
//				load_instance_proc<PFN_vkCreateDebugUtilsMessengerEXT>(state->instance, "vkCreateDebugUtilsMessengerEXT");
//			state->destroy_debug_messenger = load_instance_proc<PFN_vkDestroyDebugUtilsMessengerEXT>(state->instance, "vkDestroyDebugUtilsMessengerEXT");
//			if (!create_debug || !state->destroy_debug_messenger)
//				return fail_device_creation(state, Error::driver_error);
//			const VkDebugUtilsMessengerCreateInfoEXT debug_info{
//				.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
//				.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
//				.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
//							   VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
//				.pfnUserCallback = debug_callback,
//			};
//			error = error_from_vk(create_debug(state->instance, &debug_info, nullptr, &state->debug_messenger), "vkCreateDebugUtilsMessengerEXT");
//			if (error != Error::none)
//				return fail_device_creation(state, error);
//		}
//#endif

//#if defined(_WIN32)
//		if (presentation)
//		{
//			const VkWin32SurfaceCreateInfoKHR surface_info{
//				.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
//				.hinstance = GetModuleHandleW(nullptr),
//				.hwnd = static_cast<HWND>(desc.window),
//			};
//			error = error_from_vk(vkCreateWin32SurfaceKHR(state->instance, &surface_info, nullptr, &state->surface), "vkCreateWin32SurfaceKHR");
//			if (error != Error::none)
//				return fail_device_creation(state, error);
//		}
//#endif

		VkPhysicalDevice physical_devices[maxPhysicalDevices]{};
		uint32_t physical_device_count = 0;
		error = enumerate_physical_devices(state->vkInstance,  physical_devices, physical_device_count, maxPhysicalDevices);
		if (error != VK_SUCCESS)
			return { .error = error };

		Candidate selected{};
		bool has_selected = false;
		char candidate_diagnostics[2048] = "No compatible GPU found.";
		for (uint32_t index = 0; index < physical_device_count; ++index)
		{
			const VkPhysicalDevice physical_device = physical_devices[index];
			Candidate candidate{};
			error = inspect_candidate(physical_device, state->surface, khr_surface_maintenance1, ext_surface_maintenance1, desc, candidate);
			if (error == VK_INCOMPLETE)
			{
				//if (error_callback)
				//{
				//	VkPhysicalDeviceProperties properties{};
				//	vkGetPhysicalDeviceProperties(physical_device, &properties);
				//	append_capability_diagnostic(candidate_diagnostics, properties.deviceName,
				//		{ candidate.missing_extensions, candidate.missing_extension_count });
				//}
				//continue;
			}
			if (error != VK_SUCCESS)
				return { .error = error };
			if (!has_selected || candidate.properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
			{
				selected = candidate;
				has_selected = true;
			}
			if (candidate.properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
				break;
		}
		if (!has_selected)
			return { .error = error };

		state->physicalDevice = selected.physical_device;
		state->deviceProps = selected.properties;
		state->deviceHeapProps = selected.heap_properties;
		state->maxTimelineValueDifference = selected.vulkan12_properties.maxTimelineSemaphoreValueDifference;
		//state->deviceProps.pNext = nullptr;
		state->deviceMemProps = selected.memory_properties;
		//for (uint32_t value = 0; value < format_count; ++value)
		//{
		//	state->format_features[value] = optimal_format_features(
		//		state->physicalDevice, static_cast<Format>(value));
		//}

		DesiredFeatures enabled_features(presentation, selected.unified_image_layouts, selected.address_commands);
		state->texture_compression_etc2 = selected.texture_compression_etc2;
		enabled_features.core.features.imageCubeArray = selected.image_cube_array;
		enabled_features.core.features.samplerAnisotropy = VK_TRUE;
		enabled_features.core.features.shaderInt16 = VK_TRUE;
		enabled_features.core.features.depthBiasClamp = VK_TRUE;
		enabled_features.core.features.independentBlend = VK_TRUE;
		enabled_features.core.features.textureCompressionBC = selected.texture_compression_bc;
		enabled_features.core.features.textureCompressionASTC_LDR = selected.texture_compression_astc;
		enabled_features.core.features.textureCompressionETC2 = selected.texture_compression_etc2;
		enabled_features.core.features.fragmentStoresAndAtomics = VK_TRUE;
		enabled_features.core.features.vertexPipelineStoresAndAtomics = VK_TRUE;
		enabled_features.core.features.shaderStorageImageReadWithoutFormat = VK_TRUE;
		enabled_features.core.features.shaderStorageImageWriteWithoutFormat = VK_TRUE;
		enabled_features.core.features.multiDrawIndirect = VK_TRUE;
		enabled_features.core.features.drawIndirectFirstInstance = VK_TRUE;
		enabled_features.vulkan11.storageBuffer16BitAccess = VK_TRUE;
		enabled_features.vulkan11.storagePushConstant16 = VK_TRUE;
		enabled_features.vulkan11.storageInputOutput16 = selected.storage_input_output16;
		enabled_features.vulkan11.shaderDrawParameters = VK_TRUE;
		enabled_features.vulkan12.shaderFloat16 = VK_TRUE;
		enabled_features.vulkan12.scalarBlockLayout = VK_TRUE;
		enabled_features.vulkan12.timelineSemaphore = VK_TRUE;
		enabled_features.vulkan12.hostQueryReset = selected.timestamps[0] || selected.timestamps[1] || selected.timestamps[2];
		enabled_features.vulkan12.bufferDeviceAddress = VK_TRUE;
		enabled_features.vulkan13.synchronization2 = VK_TRUE;
		enabled_features.vulkan13.dynamicRendering = VK_TRUE;
		enabled_features.vulkan13.maintenance4 = VK_TRUE;
		enabled_features.vulkan14.maintenance5 = VK_TRUE;
		enabled_features.descriptorHeap.descriptorHeap = VK_TRUE;
		enabled_features.addressCommands.deviceAddressCommands = selected.address_commands;
		enabled_features.untypedPointers.shaderUntypedPointers = VK_TRUE;
		enabled_features.unifiedImageLayouts.unifiedImageLayouts = selected.unified_image_layouts ? VK_TRUE : VK_FALSE;
		enabled_features.meshShader.taskShader = VK_TRUE;
		enabled_features.meshShader.meshShader = VK_TRUE;
		enabled_features.swapchainMaintenance1.swapchainMaintenance1 = VK_TRUE;

		const uint32_t requested_counts[]{ desc.desiredGraphicsQueues, desc.desiredComputeQueues, desc.desiredTransferQueues };
		uint32_t queue_counts[queueTypeCount]{};
		for (uint32_t type = 0; type < queueTypeCount; ++type)
		{
			queue_counts[type] = requested_counts[type] < selected.queue_counts[type] ? requested_counts[type] : selected.queue_counts[type];
			state->queueCount += queue_counts[type];
		}
		state->queues = new Queue[state->queueCount];
		float* queue_priorities = new float[state->queueCount];
		for (uint32_t index = 0; index < state->queueCount; ++index)
			queue_priorities[index] = 1.0f;
		VkDeviceQueueCreateInfo queue_infos[queueTypeCount]{};
		uint32_t first_queue = 0;
		for (uint32_t type = 0; type < queueTypeCount; ++type)
		{
			if (queue_counts[type] == 0) continue;
			state->queueFamilies[state->queueFamilyCount] = selected.queue_families[type];
			queue_infos[state->queueFamilyCount++] = {
				.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
				.queueFamilyIndex = selected.queue_families[type],
				.queueCount = queue_counts[type],
				.pQueuePriorities = queue_priorities + first_queue,
			};
			first_queue += queue_counts[type];
		}
		const char* enabled_device_extensions[7]{};
		uint32_t enabled_device_extension_count = 0;
		enabled_device_extensions[enabled_device_extension_count++] = VK_EXT_DESCRIPTOR_HEAP_EXTENSION_NAME;
		if (selected.address_commands) enabled_device_extensions[enabled_device_extension_count++] = VK_KHR_DEVICE_ADDRESS_COMMANDS_EXTENSION_NAME;
		enabled_device_extensions[enabled_device_extension_count++] = VK_KHR_SHADER_UNTYPED_POINTERS_EXTENSION_NAME;
		if (selected.unified_image_layouts)
		{
			enabled_device_extensions[enabled_device_extension_count++] = VK_KHR_UNIFIED_IMAGE_LAYOUTS_EXTENSION_NAME;
		}
		enabled_device_extensions[enabled_device_extension_count++] = VK_EXT_MESH_SHADER_EXTENSION_NAME;
#if defined(_WIN32)
		if (presentation)
		{
			enabled_device_extensions[enabled_device_extension_count++] = VK_KHR_SWAPCHAIN_EXTENSION_NAME;
			enabled_device_extensions[enabled_device_extension_count++] = selected.khr_swapchain_maintenance1
				? VK_KHR_SWAPCHAIN_MAINTENANCE_1_EXTENSION_NAME
				: VK_EXT_SWAPCHAIN_MAINTENANCE_1_EXTENSION_NAME;
		}
#endif
		const VkDeviceCreateInfo device_info{
			.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
			.pNext = &enabled_features.core,
			.queueCreateInfoCount = state->queueFamilyCount,
			.pQueueCreateInfos = queue_infos,
			.enabledExtensionCount = enabled_device_extension_count,
			.ppEnabledExtensionNames = enabled_device_extensions,
		};
		error = vkCreateDevice(state->physicalDevice, &device_info, nullptr, &state->device);
		delete[] queue_priorities;
		if (error != VK_SUCCESS)
			return { .error = error };
		first_queue = 0;
		for (uint32_t type = 0; type < queueTypeCount; ++type)
		{
			for (uint32_t index = 0; index < queue_counts[type]; ++index)
			{
				Queue& queue = state->queues[first_queue++];
				queue.familyIndex = selected.queue_families[type];
				queue.timestamps = selected.timestamps[type];
				vkGetDeviceQueue(state->device, queue.familyIndex, index, &queue.queue);
			}
		}
		//if (!supports_gpu_heap_memory(*state) || !select_texture_memory_type(*state))
		//	return fail_device_creation(state, Error::unsupported);

		state->fn.writeSamplerDescriptors = loadDeviceProc<PFN_vkWriteSamplerDescriptorsEXT>(state->device, "vkWriteSamplerDescriptorsEXT");
		state->fn.writeResourceDescriptors = loadDeviceProc<PFN_vkWriteResourceDescriptorsEXT>(state->device, "vkWriteResourceDescriptorsEXT");
		state->fn.cmdBindSamplerHeap = loadDeviceProc<PFN_vkCmdBindSamplerHeapEXT>(state->device, "vkCmdBindSamplerHeapEXT");
		state->fn.cmdBindTextureHeap = loadDeviceProc<PFN_vkCmdBindResourceHeapEXT>(state->device, "vkCmdBindResourceHeapEXT");
		state->fn.cmdPushData = loadDeviceProc<PFN_vkCmdPushDataEXT>(state->device, "vkCmdPushDataEXT");
		state->fn.cmdDrawMeshTasks = loadDeviceProc<PFN_vkCmdDrawMeshTasksEXT>(state->device, "vkCmdDrawMeshTasksEXT");
		if (!state->fn.writeSamplerDescriptors || !state->fn.writeResourceDescriptors ||
			!state->fn.cmdBindSamplerHeap || !state->fn.cmdBindTextureHeap ||
			!state->fn.cmdPushData || !state->fn.cmdDrawMeshTasks)
		{
			return { .error = VK_INCOMPLETE };
		}
		if (selected.address_commands)
		{
			state->fn.cmdBindIndexBuffer = loadDeviceProc<PFN_vkCmdBindIndexBuffer3KHR>(state->device, "vkCmdBindIndexBuffer3KHR");
			state->fn.cmdDrawIndirect = loadDeviceProc<PFN_vkCmdDrawIndirect2KHR>(state->device, "vkCmdDrawIndirect2KHR");
			state->fn.cmdDrawIndexedIndirect = loadDeviceProc<PFN_vkCmdDrawIndexedIndirect2KHR>(state->device, "vkCmdDrawIndexedIndirect2KHR");
			state->fn.cmdDispatchIndirect = loadDeviceProc<PFN_vkCmdDispatchIndirect2KHR>(state->device, "vkCmdDispatchIndirect2KHR");
			state->fn.cmdDrawMeshTasksIndirect = loadDeviceProc<PFN_vkCmdDrawMeshTasksIndirect2EXT>(state->device, "vkCmdDrawMeshTasksIndirect2EXT");
			state->fn.cmdCopyMemory = loadDeviceProc<PFN_vkCmdCopyMemoryKHR>(state->device, "vkCmdCopyMemoryKHR");
			state->fn.cmdCopyMemoryToImage = loadDeviceProc<PFN_vkCmdCopyMemoryToImageKHR>(state->device, "vkCmdCopyMemoryToImageKHR");
			state->fn.cmdCopyImageToMemory = loadDeviceProc<PFN_vkCmdCopyImageToMemoryKHR>(state->device, "vkCmdCopyImageToMemoryKHR");
			if (!state->fn.cmdBindIndexBuffer || !state->fn.cmdDrawIndirect || !state->fn.cmdDrawIndexedIndirect ||
				!state->fn.cmdDispatchIndirect || !state->fn.cmdDrawMeshTasksIndirect || !state->fn.cmdCopyMemory ||
				!state->fn.cmdCopyMemoryToImage || !state->fn.cmdCopyImageToMemory)
			{
				return { .error = VK_INCOMPLETE };
			}
		}
		else
		{
			//state->fn.cmdDrawMeshTasksIndirectBuffer = loadDeviceProc<PFN_vkCmdDrawMeshTasksIndirectEXT>(state->device, "vkCmdDrawMeshTasksIndirectEXT");
			//if (!state->fn.cmdDrawMeshTasksIndirectBuffer)
			//	return { .error = VK_INCOMPLETE };
			//state->buffer_map = new detail::BufferAddressMap{};
		}

		if (presentation)
		{
			const VkSemaphoreTypeCreateInfo type_info{
				.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
				.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE,
			};
			const VkSemaphoreCreateInfo semaphore_info{
				.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
				.pNext = &type_info,
			};
			error = vkCreateSemaphore(state->device, &semaphore_info, nullptr, &state->presentationRetirement);
			if (error != VK_SUCCESS)
				return { .error = VK_INCOMPLETE };
			for (uint32_t index = 0; index < state->presentContextCount; ++index)
			{
				error = state->createPresentContext(state->presentContexts[index]);
				if (error != VK_SUCCESS)
					return { .error = VK_INCOMPLETE };
			}
		}

		state->capabilities = {
			.deviceName = state->deviceProps.deviceName,
			.queueCount = state->queueCount,
			.queueGeneralCount = queue_counts[0],
			.queueComputeCount = queue_counts[1],
			.queueTransferCount = queue_counts[2],
			//.copy_texture_granularity = selected.copy_texture_granularity,
			.maxPushDataSize = state->deviceHeapProps.maxPushDataSize,
			.textureHeapAlignment = state->textureHeapAlignment,
			.timestampPeriodns = selected.properties.limits.timestampPeriod,
			.subTexelPrecisionBits = selected.properties.limits.subTexelPrecisionBits,
			.textureCompressionBC = selected.texture_compression_bc,
			.textureCompressionASTC = selected.texture_compression_astc,
			.storageInputOutput16 = selected.storage_input_output16,
			.indirectMeshDraw = true,
		};
		if (presentation)
		{
			state->swapChain = new Swapchain;
			state->swapChain->state = state;
			state->swapChain->format = desc.swapchain_format;
			state->swapChain->colorSpace = desc.colorSpace;
			error = recreateSwapchain(*state->swapChain); // THIS IS BROKE
			if (error != VK_SUCCESS)
				return { .error = VK_INCOMPLETE };
		}
		return {
			.device = state
		};
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

	void draw(CommandBuffer* commands, const void* root, uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex,
		uint32_t firstInstance)
	{
		emitRootPointer(commands, root);
		vkCmdDraw(commands->commandBuffer, vertexCount, instanceCount, firstVertex, firstInstance);
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
			recordImageBarriers(commandBuffer, barriers.data(), 1);
		}
		VkResult endCommandRecordingResult = vkEndCommandBuffer(commandBuffer);
		if (endCommandRecordingResult != VK_SUCCESS)
		{
			std::cout << "Failed to end command recording. Error code: " << endCommandRecordingResult << std::endl;
		}
	}

	void recordImageBarriers(VkCommandBuffer commandBuffer, const VkImageMemoryBarrier2* barriers, uint32_t numBarriers)
	{
		const VkDependencyInfo dependency{
			.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
			.imageMemoryBarrierCount = (uint32_t)numBarriers,
			.pImageMemoryBarriers = barriers,
		};
		vkCmdPipelineBarrier2(commandBuffer, &dependency);
	}

	CommandPool* createCommandPool(Device* device, uint32_t queueIndex)
	{
		CommandPool* pool = new CommandPool{ .state = device, .timestamps = device->queues[queueIndex].timestamps };
		const VkCommandPoolCreateInfo pool_info{
			.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
			.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
			.queueFamilyIndex = device->queues[queueIndex].familyIndex,
		};
		if (vkCreateCommandPool(device->device, &pool_info, nullptr, &pool->commandPool) != VK_SUCCESS)
		{
			delete pool;
			return nullptr;
		}
		return pool;
	}

	void gpuBeginRenderPass(CommandBuffer* buffer, const RenderingDesc& desc, RenderingFlags flags)
	{

		const RenderView* areaView = desc.colors.size() ? desc.colors.at(0).renderView
			: (desc.depth.renderView ? desc.depth.renderView : desc.stencil.renderView);
		// Fix renderView
		VkRenderingAttachmentInfo colorAttatchments[maxColorAttachments];
		for (size_t index = 0; index < maxColorAttachments; ++index)
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

	void submitCommands(Device* device, const SubmitDesc& desc, uint32_t queue_index, VkSemaphore wait_semaphore, VkSemaphore signal_semaphore)
	{
		Queue* queue = &device->queues[queue_index];
		TimelineSemaphore* completion = desc.completion.semaphore;
		//assert(completion);
		//assert(desc.waits.data || desc.waits.size == 0);
		size_t command_count = desc.commands.size();
		for (size_t index = 0; index < desc.commands.size(); ++index)
		{
			//assert(desc.commands.data[index]);
			if (desc.commands.at(index)->has_epilogue) ++command_count;
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
			assert(commands);
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
			if (commands->has_epilogue)
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
			assert(point.semaphore);
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

	//void submitAndPresent(Device* device, const present::SubmitDesc& desc)
	//{
	//	Swapchain* currentChain = device->swapChain;

	//	PresentContext& context = *currentChain->presentContext;
	//}

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


	bool swapchainSurfaceConfigurationChanged(const Swapchain& swapchain)
	{
		VkSurfaceCapabilitiesKHR surfaceCapabilities{};
		VkResult getCapsResults = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(swapchain.state->physicalDevice, swapchain.state->surface, &surfaceCapabilities);


		const VkExtent2D extent = surfaceCapabilities.currentExtent;
		const uint32_t variable_extent = UINT_MAX;
		return extent.width == variable_extent ||
			extent.height == variable_extent ||
			extent.width != swapchain.width ||
			extent.height != swapchain.height ||
			surfaceCapabilities.currentTransform != swapchain.transform ||
			chooseCompositeAlpha(surfaceCapabilities.supportedCompositeAlpha) != swapchain.compositeAlpha;
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

	SwapchainFrame acquire(CommandBuffer* commands)
	{
		Device* device = commands->state;
		device->pollPresentationRetirement();


		Swapchain* swapchain = device->swapChain;
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
			swapchain->recreateRequired = result == VK_SUBOPTIMAL_KHR && swapchain_surface_configuration_changed(*swapchain);
			device->acquiredSwapChain = swapchain;
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
			recordImageBarriers(commands->commandBuffer, &barrier, 1 );
			commands->swapchain = swapchain;
			swapchain->transitionCommands = commands;
			return {
				.renderView = &swapchain->renderViews[image_index],
				.extent = { .x = swapchain->width, .y = swapchain->height },
			};
		}
	}

	bool swapchain_surface_configuration_changed(const Swapchain& swapchain)
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

	void Device::waitPresentContext(PresentContext& context) noexcept
	{
		if (!context.presentPending)
			return;
		vkWaitForFences(device, 1, &context.presented, VK_TRUE, ~uint64_t{ 0 });
		context.presentPending = false;
		finishPresentContext(context);
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

	void Device::destroyPresentContext(PresentContext& context) noexcept
	{
		//assert(!context.present_pending);
		if (device && context.acquired) vkDestroySemaphore(device, context.acquired, nullptr);
		if (device && context.rendered) vkDestroySemaphore(device, context.rendered, nullptr);
		if (device && context.presented) vkDestroyFence(device, context.presented, nullptr);
		context = {};
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

	VkResult Device::createPresentContext(PresentContext& context)
	{
		//assert(!context.acquired && !context.rendered && !context.presented && !context.swapchain && !context.present_pending);
		const VkSemaphoreCreateInfo semaphore_info{
			.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
		};
		VkResult error = vkCreateSemaphore(device, &semaphore_info, nullptr, &context.acquired);
		if (error != VK_SUCCESS)
			return error;
		error = vkCreateSemaphore(device, &semaphore_info, nullptr, &context.rendered);
		if (error != VK_SUCCESS)
		{
			destroyPresentContext(context);
			return error;
		}
		const VkFenceCreateInfo fence_info{
			.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
		};
		error = vkCreateFence(device, &fence_info, nullptr, &context.presented);
		if (error != VK_SUCCESS)
		{
			destroyPresentContext(context);
			return error;
		}
		return VK_SUCCESS;
	}

	

	void destroyOwnedSwapchain(Device& device) noexcept;

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

	VkResult recreateSwapchain(Swapchain& swapchain)
	{
		Device& device = *swapchain.state;
		//assert(!swapchain.acquired && !device.acquired_swapchain);
		const VkSurfacePresentModeKHR present_mode_info{
			.sType = VK_STRUCTURE_TYPE_SURFACE_PRESENT_MODE_KHR,
			.presentMode = presentMode,
		};
		const VkPhysicalDeviceSurfaceInfo2KHR surface_info{
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SURFACE_INFO_2_KHR,
			.pNext = &present_mode_info,
			.surface = device.surface,
		};
		VkSurfaceCapabilities2KHR capabilities_info{
			.sType = VK_STRUCTURE_TYPE_SURFACE_CAPABILITIES_2_KHR,
		};
		VkResult error = vkGetPhysicalDeviceSurfaceCapabilities2KHR(device.physicalDevice, &surface_info, &capabilities_info);
		if (error != VK_SUCCESS)
			return error;
		const VkSurfaceCapabilitiesKHR& capabilities = capabilities_info.surfaceCapabilities;

		const VkExtent2D extent = capabilities.currentExtent;
		if (extent.width == UINT_MAX)
			return VK_INCOMPLETE;
		if ((capabilities.supportedUsageFlags & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) == 0)
		{
			return VK_INCOMPLETE;
		}

		VkSurfaceFormatKHR formats[max_surface_formats]{};
		uint32_t surface_format_count = 0;
		error = vkGetPhysicalDeviceSurfaceFormatsKHR(device.physicalDevice, device.surface, &surface_format_count, nullptr);
		if (error != VK_SUCCESS)
			return error;
		if (surface_format_count == 0 || surface_format_count > max_surface_formats)
			return VK_INCOMPLETE;
		error = vkGetPhysicalDeviceSurfaceFormatsKHR(device.physicalDevice, device.surface, &surface_format_count, formats);
		if (error != VK_SUCCESS)
			return error;

		const VkFormat requested_format = to_vk(swapchain.format);
		bool format_supported = false;
		for (uint32_t index = 0; index < surface_format_count; ++index)
		{
			if ((formats[index].format == requested_format ||
				formats[index].format == VK_FORMAT_UNDEFINED) &&
				formats[index].colorSpace == swapchain.colorSpace)
			{
				format_supported = true;
				break;
			}
		}
		if (!format_supported)
			return VK_INCOMPLETE;

		if (extent.width == 0 || extent.height == 0)
		{
			swapchain.width = 0;
			swapchain.height = 0;
			swapchain.recreateRequired = true;
			return VK_ERROR_INITIALIZATION_FAILED;
		}

		uint32_t requested_image_count = device.presentContextCount;
		if (requested_image_count < capabilities.minImageCount) requested_image_count = capabilities.minImageCount;
		if (capabilities.maxImageCount != 0 && requested_image_count > capabilities.maxImageCount) requested_image_count = capabilities.maxImageCount;
		if (requested_image_count == 0 || requested_image_count > maxSwapchainImages) return VK_ERROR_INITIALIZATION_FAILED;

		const VkCompositeAlphaFlagBitsKHR composite_alpha = chooseCompositeAlpha(capabilities.supportedCompositeAlpha);
		if (!composite_alpha)
		{
			//if (error_callback) error_callback("The display surface exposes no supported composite alpha mode.", false);
			return VK_INCOMPLETE;
		}
		const VkSwapchainKHR old_handle = swapchain.handle;
		const VkSwapchainPresentModesCreateInfoKHR present_modes_info{
			.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_PRESENT_MODES_CREATE_INFO_KHR,
			.presentModeCount = 1,
			.pPresentModes = &presentMode,
		};
		const VkSwapchainCreateInfoKHR create_info{
			.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
			.pNext = &present_modes_info,
			.surface = device.surface,
			.minImageCount = requested_image_count,
			.imageFormat = requested_format,
			.imageColorSpace = swapchain.colorSpace,
			.imageExtent = extent,
			.imageArrayLayers = 1,
			.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
			.imageSharingMode = device.queueFamilyCount > 1 ? VK_SHARING_MODE_CONCURRENT : VK_SHARING_MODE_EXCLUSIVE,
			.queueFamilyIndexCount = device.queueFamilyCount,
			.pQueueFamilyIndices = device.queueFamilies,
			.preTransform = capabilities.currentTransform,
			.compositeAlpha = composite_alpha,
			.presentMode = presentMode,
			.clipped = VK_TRUE,
			.oldSwapchain = old_handle,
		};
		VkSwapchainKHR new_handle = VK_NULL_HANDLE;
		error = vkCreateSwapchainKHR(device.device, &create_info, nullptr, &new_handle);
		if (error != VK_SUCCESS)
		{
			retireSwapchainHandle(swapchain);
			return error;
		}

		VkImage images[maxSwapchainImages]{};
		uint32_t image_count = 0;
		VkResult result = vkGetSwapchainImagesKHR(device.device, new_handle, &image_count, nullptr);
		if (result != VK_SUCCESS || image_count == 0 || image_count > maxSwapchainImages)
		{
			vkDestroySwapchainKHR(device.device, new_handle, nullptr);
			retireSwapchainHandle(swapchain);
			return result;
		}
		result = vkGetSwapchainImagesKHR(device.device, new_handle, &image_count, images);
		if (result != VK_SUCCESS)
		{
			vkDestroySwapchainKHR(device.device, new_handle, nullptr);
			retireSwapchainHandle(swapchain);
			return result;
		}

		VkImageView views[maxSwapchainImages]{};
		for (uint32_t index = 0; index < image_count; ++index)
		{
			const VkImageViewCreateInfo view_info{
				.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
				.image = images[index],
				.viewType = VK_IMAGE_VIEW_TYPE_2D,
				.format = requested_format,
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
				vkDestroySwapchainKHR(device.device, new_handle, nullptr);
				retireSwapchainHandle(swapchain);
				return result;
			}
		}

		retireSwapchainHandle(swapchain);
		swapchain.handle = new_handle;
		swapchain.imageCount = image_count;
		swapchain.width = extent.width;
		swapchain.height = extent.height;
		swapchain.transform = capabilities.currentTransform;
		swapchain.compositeAlpha = composite_alpha;
		swapchain.recreateRequired = false;
		for (uint32_t index = 0; index < image_count; ++index)
		{
			swapchain.images[index] = images[index];
			swapchain.renderViews[index] = {
				.state = &device,
				.view = views[index],
				.width = extent.width,
				.height = extent.height,
				.swapchain_view = true,
			};
		}
		return VK_SUCCESS;
	}

	void destroyOwnedSwapchain(Device& device) noexcept
	{
		if (!device.swapChain)
			return;
		Swapchain* swapchain = device.swapChain;
		//assert(!swapchain->acquired && device.acquired_swapchain != swapchain);
		retireSwapchainHandle(*swapchain);
		swapchain->state = nullptr;
		delete swapchain;
		device.swapChain = nullptr;
	}

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
		for (uint32_t index = 0; index < queueCount; ++index)
		{
			free(queues[index].commandSubmitInfos);
			free(queues[index].waitSubmitInfos);
		}
		delete[] queues;
		//delete buffer_map;
		free(swapchainDeleteQueue.entries);
		for (uint32_t index = 0; index < presentContextCount; ++index)
		{
			destroyPresentContext(presentContexts[index]);
		}
		if (device) vkDestroyDevice(device, nullptr);
		if (vkInstance && surface) vkDestroySurfaceKHR(vkInstance, surface, nullptr);
		//if (instance && debug_messenger && destroyDebugMessenger) destroy_debug_messenger(instance, debug_messenger, nullptr);
		if (vkInstance) vkDestroyInstance(vkInstance, nullptr);
	}

	void setViewport(CommandBuffer* commands, const Viewport& viewport)
	{
		const VkViewport vk_viewport{
			.x = viewport.x,
			.y = viewport.y,
			.width = viewport.width,
			.height = viewport.height,
			.minDepth = viewport.min_depth,
			.maxDepth = viewport.max_depth,
		};
		vkCmdSetViewportWithCount(commands->commandBuffer, 1, &vk_viewport);
	}

	void setScissor(CommandBuffer* commands, const Scissor& scissor)
	{
		const VkRect2D vk_scissor{
			.offset = { .x = scissor.x, .y = scissor.y },
			.extent = { .width = scissor.width, .height = scissor.height },
		};
		vkCmdSetScissorWithCount(commands->commandBuffer, 1, &vk_scissor);
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

	void emitRootPointer(CommandBuffer* commands, const void* root)
	{
		if (!root) return;
		const VkPushDataInfoEXT info{
			.sType = VK_STRUCTURE_TYPE_PUSH_DATA_INFO_EXT,
			.data = { .address = &root, .size = sizeof(root) },
		};
		commands->state->fn.cmdPushData(commands->commandBuffer, &info);
	}

	void wait_timeline(TimelinePoint point)
	{
		TimelineSemaphore* semaphore = point.semaphore;
		assert((semaphore && semaphore->state && semaphore->semaphore) && "wait_timeline requires a live timeline semaphore");

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




	struct Stencil
	{

	};



	struct BlendDesc 
	{

	};

	struct RasterizerDesc
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
	void submitAndPresent(Device* device, const SubmitDesc& desc)
	{
		Swapchain* swapchain = device->swapChain;
		PresentContext& presentContext = *swapchain->presentContext;

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
		const VkResult result = vkQueuePresentKHR(device->queues[0].queue, &presentInfo);
		presentContext.presentPending = true;
		presentContext.swapchain = swapchain->handle;
		swapchain->presentContext = nullptr;
		swapchain->acquired = false;
		device->acquiredSwapChain = nullptr;
		device->nextPresentContext = (device->nextPresentContext + 1) % device->presentContextCount;
		if (result == VK_ERROR_OUT_OF_DATE_KHR)
			swapchain->recreateRequired = true;
		else if (result == VK_SUBOPTIMAL_KHR)
			swapchain->recreateRequired = swapchain->recreateRequired || swapchainSurfaceConfigurationChanged(*swapchain);
		else if (result != VK_SUCCESS)
			throw std::runtime_error("FAILED TO PRESENT swapchain images");
	}

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

	std::vector<char> readShader(std::string fileName, std::string filePath)
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

	void bindPSO(CommandBuffer* commands, const PSO* pso)
	{
		vkCmdBindPipeline(commands->commandBuffer, pso->bindPoint, pso->pso);
	}

	// TODO: currently hardcoded depth, stencil, and color target descriptors, replace later with flexible values
	// NOTE: Randomly picked values, if triangle is wrong problem is likely here
	PSO* createRasterPSO(Device* device, const ShaderStage& vertStage, const ShaderStage& fragStage, std::vector<ColorTargetDesc> colorTargets, VkFormat depthFormat, VkFormat stencilFormat,
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
			colorFormats[index] = VK_FORMAT_B8G8R8A8_SRGB;
		}
		const VkPipelineRenderingCreateInfo renderingInfo{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
			.colorAttachmentCount = maxColorAttachments,
			.pColorAttachmentFormats = colorFormats,
			.depthAttachmentFormat = depthFormat,
			.stencilAttachmentFormat = stencilFormat,
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
		return createRasterPSO(device, graphicsDesc.vertex, graphicsDesc.fragment, graphicsDesc.color_targets, graphicsDesc.depthFormat, graphicsDesc.stencilFormat, graphicsDesc.rasterization, false);
	}

	void destroyPso(PSO* pso) noexcept
	{
		delete pso;
	}

	void destroyCommandPool(CommandPool* pool) noexcept
	{
		if (!pool) return;
		while (pool->first)
		{
			CommandBuffer* commands = pool->first;
			assert(!commands->swapchain && "an acquired swapchain image must be presented before destroying its command pool");
			pool->first = commands->next;
			if (commands->timestampPool) vkDestroyQueryPool(pool->state->device, commands->timestampPool, nullptr);
			free(commands->timestampDestinations);
			free(commands->timestampResults);
			delete commands;
		}
		vkDestroyCommandPool(pool->state->device, pool->commandPool, nullptr);
		delete pool;
	}

	void destroyTimelineSemaphore(TimelineSemaphore* semaphore) noexcept
	{
		if (!semaphore) return;
		vkDestroySemaphore(semaphore->state->device, semaphore->semaphore, nullptr);
		delete semaphore;
	}

	void destroyDevice(Device* device) noexcept
	{
		delete device;
	}

	TimelineSemaphore* createTimelineSemaphore(Device* device, uint64_t initial_value) noexcept
	{
		const VkSemaphoreTypeCreateInfo timeline_info{
			.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
			.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE,
			.initialValue = initial_value,
		};
		const VkSemaphoreCreateInfo create_info{
			.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
			.pNext = &timeline_info,
		};
		TimelineSemaphore* result = new TimelineSemaphore{
			.state = device,
		};
		if (vkCreateSemaphore(device->device, &create_info, nullptr, &result->semaphore) != VK_SUCCESS)
		{
			delete result;
			return nullptr;
		}
		return result;
	}
	void waitIdle(Device* device)
	{
		vkDeviceWaitIdle(device->device);
		device->drainContexts();
		device->nextPresentContext = 0;
	}
} // gpu