/*
*	The most important methods making up the core of the Framework rendering
*	Namespace GPU are methods relating directly to the GPU such as memory methods and rendering
*	Namespace Present are methods relating to settings/descriptors for rendering
*	Namespace Pipeline for methods relating to rendering pipeline
*	TODO: Currently the pipeline methods/namespace is very strict, develop a flexible system after tech demo
*	TODO: Currently no debug method to make it easier, add later
*	TODO: Utilizes a very pointless "presentation" bool I used from noapi, rework to not use this later
*	TODO: Stencil, depth, render pass, etc. are from noapi and need to be changed to better fit what we are going for
*/


#pragma once
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

	struct GpuRange;

	struct GpuHeap;

	struct Device;

	// What we want from our GPU
	struct DesiredDevice
	{
		//void* window = nullptr;
		uint32_t desiredGraphicsQueues = 1; // Graphics(present) must be at least 1
		uint32_t desiredComputeQueues = 0;
		uint32_t desiredTransferQueues = 0;

		uint32_t desiredSwapchainImageCount = 2;
		uint32_t timeStampQueryCount = 256;

		VkColorSpaceKHR colorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;

		DesiredDevice() = default;
	};


	struct Queue;

	struct DeviceCapabilities;

	struct GpuTextureSizeAlign;

	struct Swapchain;

	struct Texture;

	struct CommandBuffer;

	struct CommandPool;

	// In no-api this is under gpu::detail namespace
	struct PresentContext;

	struct DesiredFeatures;

	// ALSO TO BE ORGNIZED LATER
	struct Viewport;
	struct Scissor;
	struct StencilFaceState
	{
		CompareOp compare = CompareOp::always;
		StencilOp fail = StencilOp::keep;
		StencilOp pass = StencilOp::keep;
		StencilOp depth_fail = StencilOp::keep;
		uint8_t reference = 0;
	};
	struct DeviceFunctions;
	struct SwapchainFrame
	{
		present::RenderView* renderView = nullptr;
		uint32x2 extent = {};
	};
	struct uint32x2
	{
		uint32_t x = 0;
		uint32_t y = 0;
	};
	//struct DeferredSwapchainImage;
	//struct SwapchainDeleteQueue;
	//struct RetiredSwapchain;
	template<typename T>
	T loadDeviceProc(VkDevice device, const char* name);





	// Setup
	uint32_t scoreDevice(VkPhysicalDevice device, VkSurfaceKHR surface);
	void pickPhysicalDevice(VkInstance instance, Device* deviceInfo);
	bool createDevice(Device* device, const DesiredDevice& desiredDevice = {}, GLFWwindow* window);

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
	VkCompositeAlphaFlagBitsKHR chooseCompositeAlpha(VkCompositeAlphaFlagsKHR supported);
	SwapchainFrame acquire(CommandBuffer* commands);
	

	// TO BE ORGANIZED BETTER LATER
	void setViewport(CommandBuffer* commands, const Viewport& viewport);
	void setScissor(CommandBuffer* commands, const Scissor& scissor);
	void setDepthStencil(CommandBuffer* commands, const present::DepthStencilState& state);
	void emitRootPointer(CommandBuffer* commands, const void* root);

	// Copy methods
	void copyMemory();
	void copyMemoryToTexture();
	void copyTextureToMemory();

	// Command Buffer methods
	CommandBuffer* beginCommands(CommandPool* pool);
	void resetCommandPool(CommandPool* pool);
	void endCommands(CommandBuffer* buffer);
	void recordImageBarriers(VkCommandBuffer commandBuffer, const std::vector<VkImageMemoryBarrier2> barriers);
	CommandPool* createCommandPool(Device* device, uint32_t queueIndex = 0);

	// Render pass methods
	void gpuBeginRenderPass(CommandBuffer* buffer, const present::RenderingDesc& desc, RenderingFlags flags = RenderingFlags::none);
	void gpuEndRenderPass(CommandBuffer* buffer);


	void submitCommands(Device* device, const present::SubmitDesc& desc, uint32_t queue_index, VkSemaphore wait_semaphore, VkSemaphore signal_semaphore);
	void submitAndPresent(Device* device, const present::SubmitDesc& desc);

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


	struct RenderView;

	struct Stencil;

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
		std::vector<CommandBuffer* const> commands;
		std::vector<const sync::TimelinePoint> waits;
		sync::TimelinePoint completion = {};
	};

	struct RenderingDesc
	{
		std::vector<const ColorAttachment> colors;
		DepthAttachment depth = {};
		StencilAttachment stencil = {};
	};

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
} // present

// Variables and methods needed to manage the graphics pipeline
namespace gpu::pipeline
{
	struct CustomPipeline;

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
	struct PSO;

	struct RasterizationState;

	std::vector<char> readShader(std::string fileName, std::string filePath);

	void bindPSO(CommandBuffer* commands, const PSO* pso);

	PSO* createRasterPSO(Device* device, const ShaderStage& vertStage, const ShaderStage& fragStage, const RasterizationState& rastState, const ShaderStage& task = {}, bool mesh);

	PSO* createGraphicsPSO(Device* device, GraphicsPSODesc graphicsDesc);
} // pipeline

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
	TimelineSemaphore* createTimelineSemaphore(Device* device, uint64_t initial_value = 0) noexcept;
	void waitIdle(Device* device);
} // sync