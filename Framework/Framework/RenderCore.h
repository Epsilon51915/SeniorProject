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


namespace gpu
{

	struct GpuRange;

	struct GpuHeap;

	struct DesiredDevice;

	struct Device;

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
	struct StencilFaceState;
	struct DeviceFunctions;
	template<typename T>
	T loadDeviceProc(VkDevice device, const char* name);


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


	// Setup
	uint32_t scoreDevice(VkPhysicalDevice device, VkSurfaceKHR surface);
	void pickPhysicalDevice(VkInstance instance, Device* deviceInfo);
	bool createDevice(Device* device, const DesiredDevice& desiredDevice, GLFWwindow* window);

	// GPU memory methods
	void* gpuMalloc(size_t bytes);
	void* gpuMalloc(size_t bytes, size_t alignment);
	void* gpuHostToDevicePointer(void* ptr);
	void gpuMemCopy();
	void gpuFree(void* ptr);
	void gpuSetActiveTextureHeapPtr();

	// GPU drawing methods
	void gpuDispatch();
	void draw(CommandBuffer* commands);
	void gpuDrawIndexed();

	// Swapchain methods
	bool swapchainSurfaceConfigurationChanged(const Swapchain& swapchain);
	VkCompositeAlphaFlagBitsKHR chooseCompositeAlpha(VkCompositeAlphaFlagsKHR supported);
	

	// TO BE ORGANIZED BETTER LATER
	void setViewport(CommandBuffer* commands, const Viewport& viewport);
	void setScissor(CommandBuffer* commands, const Scissor& scissor);
	void setDepthStencil(CommandBuffer* commands, const present::DepthStencilState& state);
	void emit_root_pointer(CommandBuffer* commands, const void* root);

	// Copy methods
	void copyMemory();
	void copyMemoryToTexture();
	void copyTextureToMemory();

	// Command Buffer methods
	CommandBuffer* beginCommands(CommandPool* pool);
	void resetCommandPool(CommandPool pool);
	void endCommands(CommandBuffer* buffer);
	void recordImageBarriers(VkCommandBuffer commandBuffer, const std::vector<VkImageMemoryBarrier2> barriers);

	// Render pass methods
	void gpuBeginRenderPass(CommandBuffer* buffer, const present::RenderingDesc& desc, RenderingFlags flags);
	void gpuEndRenderPass();


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
	struct ClearColor;

	struct RenderView;

	struct Stencil;

	struct DepthStencilDesc;

	struct BlendDesc;

	struct RasterizerDesc;

	struct RenderPassDesc;

	struct RenderingDesc;

	struct TextureDesc;

	struct SubmitDesc;

	struct ColorAttachtment;

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

	struct ShaderStage;

	struct GraphicsPSODesc;

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
	struct TimelineSemaphore;
	struct TimelinePoint;
} // sync