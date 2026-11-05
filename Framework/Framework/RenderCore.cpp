#include "RenderCore.h"


namespace gpu {
	struct GpuRange
	{

	};

	struct GpuHeap
	{

	};

	// Device, or the actual GPU we are interacting with
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

		VkImage images[maxSwapchainImages]{}; // TODO: add max_swapchain_images

		//present::RenderView renderViews[maxSwapchainImages]{}; // TODO: add max_swapchain_imagesuint32 image_count = 0;
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