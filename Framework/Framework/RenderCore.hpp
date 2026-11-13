/*
*	The most important methods making up the core of the Framework rendering
*	Namespace GPU are methods relating directly to the GPU such as memory methods and rendering
*	Namespace Present are methods relating to settings/descriptors for rendering
*	Namespace Pipeline for methods relating to rendering pipeline
*	TODO: Currently the pipeline methods/namespace is very strict, develop a flexible system after tech demo
*	TODO: Currently no debug method to make it easier, add later
*	TODO: Utilizes a very pointless "presentation" bool I used from noapi, rework to not use this later
*	TODO: Stencil, depth, render pass, etc. are from noapi and need to be changed to better fit what we are going for
*	TODO: any vkEnumerate function is being terrible, rewrite to call twice over. First to get size of array, second to fill in that array
*/


#pragma once
#define VULKAN_HPP_NO_CONSTRUCTORS
#include <vulkan/vulkan.h>
//#include <vulkan/vulkan_enums.hpp>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <iostream>
#include <cassert>
#include <vector>
#include <stdexcept>
#include <fstream>
#include <stddef.h>
#include <stdio.h>


namespace gpu
{
	// These enums correspond with the equivalent Vulkan uint8's
	enum class RenderingFlags : uint32_t
	{
		none = 0,
		suspending = 2,
		resuming = 4,
	};
	enum class LoadOp : uint8_t
	{
		load,
		clear,
		discard,
	};

	enum class StoreOp : uint8_t
	{
		store,
		discard,
	};

	enum class CompareOp : uint8_t
	{
		never,
		less,
		equal,
		less_equal,
		greater,
		not_equal,
		greater_equal,
		always,
	};

	enum class StencilOp : uint8_t
	{
		keep,
		zero,
		replace,
		increment_clamp,
		decrement_clamp,
		invert,
		increment_wrap,
		decrement_wrap,
	};

	enum class Format : uint8_t
	{
		r8_srgb,
		rg8_srgb,
		rgba8_srgb,
		bgra8_srgb,

		rgba4_unorm,
		r5g5b5a1_unorm,
		r5g6b5_unorm,

		r8_unorm,
		rg8_unorm,
		rgba8_unorm,
		bgra8_unorm,
		r16_unorm,
		rg16_unorm,
		rgba16_unorm,

		r8_uint,
		rg8_uint,
		rgba8_uint,
		bgra8_uint,
		r16_uint,
		rg16_uint,
		rgba16_uint,
		r32_uint,
		rg32_uint,
		rgba32_uint,

		r16_float,
		rg16_float,
		rgba16_float,
		r32_float,
		rg32_float,
		rgba32_float,

		rgb10a2_unorm,
		rg11b10_float,

		d16_unorm,
		d24_unorm_s8_uint,
		d32_float,
		s8_uint,
		d32_float_s8_uint,

		eac_rg,
		astc_4x4_srgb,
		astc_4x4_unorm,
		bc3_srgb,
		bc3_unorm,
		bc5_rg,
		bc6h_ufloat,
		bc6h_sfloat,
		bc7_srgb,
		bc7_unorm,

		undefined, // Must remain last; preceding values are concrete texture formats.
	};


	struct RenderView;

	struct GpuRange;

	struct GpuHeap;

	// In no-api this is under gpu::detail namespace
	struct PresentContext
	{
		VkSemaphore acquired = VK_NULL_HANDLE;
		VkSemaphore rendered = VK_NULL_HANDLE;
		VkFence presented = VK_NULL_HANDLE;
		VkSwapchainKHR swapchain = VK_NULL_HANDLE;
		bool presentPending = false;
	};

	struct uint32x3
	{
		uint32_t x = 0;
		uint32_t y = 0;
		uint32_t z = 0;
	};

	struct DeviceCapabilities
	{
		const char* deviceName = "";
		// Queue Capabilities
		uint32_t queueCount = 0;
		uint32_t queueGeneralCount = 0;
		uint32_t queueComputeCount = 0;
		uint32_t queueTransferCount = 0; //  Also called copy queue in no_api
		uint32_t timestampQueryCount = 0;
		// TODO: add compression texture stuff here if we go that route

		uint32x3 copy_texture_granularity = { .x = 1, .y = 1, .z = 1 };

		uint64_t maxPushDataSize = 0;
		uint64_t textureHeapAlignment = 0;
		float timestampPeriodns = 0.0f; // Nanoseconds per timestamp tick; zero when profiling is unavailable.
		uint32_t subTexelPrecisionBits = 0; // Fractional filtering precision, for conservative sampled-field bounds.
		bool textureCompressionBC = false;
		bool textureCompressionASTC = false;
		bool storageInputOutput16 = false;
		bool indirectMeshDraw = false;
	};

	struct Device;

	// What we want from our GPU
	struct DesiredDevice
	{
		GLFWwindow* window = nullptr; // HWND on Windows; CAMetalLayer* on macOS/iOS. Null creates a headless device.
		//void* window = nullptr;
		uint32_t desiredGraphicsQueues = 1; // Graphics(present) must be at least 1
		uint32_t desiredComputeQueues = 0;
		uint32_t desiredTransferQueues = 0;

		uint32_t desiredSwapchainImageCount = 2;
		uint32_t timestamp_query_count = 256;

		VkColorSpaceKHR colorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
		Format swapchain_format = Format::undefined;
	};


	struct Queue;



	struct GpuTextureSizeAlign;

	struct Swapchain;

	struct Texture;

	struct CommandBuffer;

	struct CommandPool;

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



	struct DesiredFeatures;

	// ALSO TO BE ORGNIZED LATER
	struct Viewport
	{
		float x = 0.0f;
		float y = 0.0f;
		float width = 1.0f;
		float height = 1.0f;
		float min_depth = 0.0f;
		float max_depth = 1.0f;
	};
	struct Scissor
	{
		int32_t x = 0;
		int32_t y = 0;
		uint32_t width = 1;
		uint32_t height = 1;
	};
	struct StencilFaceState
	{
		CompareOp compare = CompareOp::always;
		StencilOp fail = StencilOp::keep;
		StencilOp pass = StencilOp::keep;
		StencilOp depth_fail = StencilOp::keep;
		uint8_t reference = 0;
	};
	struct DeviceFunctions;
	struct uint32x2
	{
		uint32_t x = 0;
		uint32_t y = 0;
	};
	struct SwapchainFrame
	{
		RenderView* renderView = nullptr;
		uint32x2 extent = {};
	};
	//struct DeferredSwapchainImage;
	//struct SwapchainDeleteQueue;
	//struct RetiredSwapchain;
	template<typename T>
	T loadDeviceProc(VkDevice device, const char* name);


	struct ClearColor
	{
		float r = 0.0f;
		float g = 0.0f;
		float b = 0.0f;
		float a = 0.0f;
	};

	struct DepthStencilState
	{
		bool depthTest = false;
		bool depthWrite = false;
		CompareOp depth_compare = CompareOp::less_equal;
		bool stencil_test = false;
		uint8_t stencil_read_mask = 0xff;
		uint8_t stencil_write_mask = 0xff;
		StencilFaceState front = {};
		StencilFaceState back = {};
	};

	struct ColorAttachment
	{
		RenderView* renderView = nullptr;
		LoadOp load = LoadOp::load;
		StoreOp store = StoreOp::store;
		ClearColor clear = {};
	};

	struct DepthAttachment
	{
		RenderView* renderView = nullptr;
		LoadOp load = LoadOp::load;
		StoreOp store = StoreOp::store;
		float clear = 1.0f;
	};

	struct StencilAttachment
	{
		RenderView* renderView = nullptr;
		LoadOp load = LoadOp::load;
		StoreOp store = StoreOp::store;
		uint8_t clear = 0;
	};

	struct BlendDesc;

	struct RasterizerDesc;

	struct RenderPassDesc;

	struct SubmitDesc
	{
		std::vector<CommandBuffer*> commands;
		std::vector<TimelinePoint> waits;
		TimelinePoint completion = {};
	};

	struct RenderingDesc
	{
		std::vector<ColorAttachment> colors;
		DepthAttachment depth = {};
		StencilAttachment stencil = {};
	};

	struct CustomPipeline;

	struct ShaderStage
	{
		const char* stageName = nullptr;
		const char* entryPoint = nullptr;
		// NOTE: noapi states threadgroups variable is for Metal

		std::vector<char> code;
	};
	struct RasterizationState
	{
		VkCullModeFlagBits cull = VK_CULL_MODE_NONE;
		float depth_bias_constant = 0.0f;
		float depth_bias_clamp = 0.0f;
		float depth_bias_slope = 0.0f;
	};
	struct BlendComponentState
	{
		VkBlendFactor source = VK_BLEND_FACTOR_ONE;
		VkBlendFactor destination = VK_BLEND_FACTOR_ZERO;
		VkBlendOp operation = VK_BLEND_OP_ADD;
	};
	struct BlendState
	{
		bool enabled = false;
		BlendComponentState color = {};
		BlendComponentState alpha = {};
	};

	struct ColorTargetDesc
	{
		VkFormat format = VK_FORMAT_UNDEFINED;
		BlendState blend = {};
		uint8_t write_mask = 0xf;
	};


	struct GraphicsPSODesc
	{
		ShaderStage vertex = {};
		ShaderStage fragment = {};
		std::vector<ColorTargetDesc> color_targets = {};
		VkFormat depthFormat = VK_FORMAT_UNDEFINED;
		VkFormat stencilFormat = VK_FORMAT_UNDEFINED;
		RasterizationState rasterization = {};
	};

	struct DeviceInit
	{
		Device* device = nullptr;
		VkResult error = VK_SUCCESS;
	};

	// Setup
	uint32_t scoreDevice(VkPhysicalDevice device, VkSurfaceKHR surface);
	void pickPhysicalDevice(VkInstance instance, Device* deviceInfo);
	DeviceInit createDevice(const DesiredDevice& desiredDevice = {}, GLFWwindow* window = nullptr);

	DeviceInit createDevice2(const DesiredDevice& desiredDevice = {}, GLFWwindow* window = nullptr);
	DeviceInit create_device(const DesiredDevice& desc);

	// GPU memory methods
	void* gpuMalloc(size_t bytes);
	void* gpuMalloc(size_t bytes, size_t alignment);
	void* gpuHostToDevicePointer(void* ptr);
	void gpuMemCopy();
	void gpuFree(void* ptr);
	void gpuSetActiveTextureHeapPtr();

	// GPU drawing methods
	void gpuDispatch();
	void draw(CommandBuffer* commands, const void* root, uint32_t vertexCount, uint32_t instanceCount = 1, uint32_t firstVertex = 0,
		uint32_t firstInstance = 0);
	void gpuDrawIndexed();

	// Swapchain methods
	bool swapchainSurfaceConfigurationChanged(const Swapchain& swapchain);
	VkResult recreateSwapchain(Swapchain& swapchain);
	VkCompositeAlphaFlagBitsKHR chooseCompositeAlpha(VkCompositeAlphaFlagsKHR supported);
	SwapchainFrame acquire(CommandBuffer* commands);
	VkResult set_swapchain_format(Device* device, Format format);
	bool swapchain_surface_configuration_changed(const Swapchain& swapchain);


	// TO BE ORGANIZED BETTER LATER
	void setViewport(CommandBuffer* commands, const Viewport& viewport);
	void setScissor(CommandBuffer* commands, const Scissor& scissor);
	void setDepthStencil(CommandBuffer* commands, const DepthStencilState& state);
	void emitRootPointer(CommandBuffer* commands, const void* root);
	void wait_timeline(TimelinePoint point);

	// Copy methods
	void copyMemory();
	void copyMemoryToTexture();
	void copyTextureToMemory();

	// Command Buffer methods
	CommandBuffer* beginCommands(CommandPool* pool);
	void resetCommandPool(CommandPool* pool);
	void endCommands(CommandBuffer* buffer);
	void recordImageBarriers(VkCommandBuffer commandBuffer, const VkImageMemoryBarrier2* barriers, uint32_t numBarriers);
	CommandPool* createCommandPool(Device* device, uint32_t queueIndex = 0);

	// Render pass methods
	void gpuBeginRenderPass(CommandBuffer* buffer, const RenderingDesc& desc, RenderingFlags flags = RenderingFlags::none);
	void gpuEndRenderPass(CommandBuffer* buffer);


	void submitCommands(Device* device, const SubmitDesc& desc, uint32_t queue_index, VkSemaphore wait_semaphore, VkSemaphore signal_semaphore);


	// GPU Queue methods
	void gpuCreateQueue();
	void gpuStartCommandRecording(Queue& queue);
	void gpuSubmitCommandBuffers();


	// Cleanup methods
	void destroyCommandPools();
	void destroySwapchain();
	void destroyHeap();
	void destroyDevice();
	void cleanup();



		

	struct Stencil;



	struct TextureDesc;



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
	void submitAndPresent(Device* device, const SubmitDesc& desc);

	// Variables and methods needed to manage the graphics pipeline

	// bar-for-bar from no-api github
	// TODO: will probably need to be changed in the future
	struct PSO;

	

	std::vector<char> readShader(std::string fileName, std::string filePath);

	void bindPSO(CommandBuffer* commands, const PSO* pso);

	//PSO* createRasterPSO(Device* device, const ShaderStage& vertStage, const ShaderStage& fragStage, const RasterizationState& rastState, const ShaderStage& task = {}, bool mesh);

	PSO* createGraphicsPSO(Device* device, GraphicsPSODesc graphicsDesc);
	void destroyPso(PSO* pso) noexcept;
	void destroyCommandPool(CommandPool* pool) noexcept;
	void destroyTimelineSemaphore(TimelineSemaphore* semaphore) noexcept;
	void destroyDevice(Device* device) noexcept;


	TimelineSemaphore* createTimelineSemaphore(Device* device, uint64_t initial_value = 0) noexcept;
	void waitIdle(Device* device);
} // gpu