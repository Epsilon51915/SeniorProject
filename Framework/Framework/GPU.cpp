#include "GPU.h"




namespace gpu {
	struct GpuRange
	{

	};

	struct GpuHeap
	{

	};

	struct Device
	{
		VkDevice device = VK_NULL_HANDLE;
		VkSurfaceKHR surface = VK_NULL_HANDLE;
		uint32_t queueCount = 0;
		uint32_t queueFamilyCount = 0;
		uint32_t textureMemoryType = VK_MAX_MEMORY_TYPES;
		uint64_t textureHeapAlignment = 16;

		Swapchain* swapChain = nullptr;
		Swapchain* acquiredSwapChain = nullptr;
	};

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

	struct Swapchain
	{
		Device* state = nullptr;
		VkSwapchainKHR handle = VK_NULL_HANDLE;

		VkImage images[maxSwapchainImages]{}; // TODO: add max_swapchain_images

		present::RenderView renderViews[maxSwapchainImages]{}; // TODO: add max_swapchain_imagesuint32 image_count = 0;
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


	void gpuBeginRenderPass(CommandBuffer* buffer, const present::RenderingDesc& renderDesc)
	{

		VkRenderingInfo renderInfo;

		vkCmdBeginRendering(buffer->commandBuffer, &renderInfo);
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


} // gpu