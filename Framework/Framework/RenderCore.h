/*
*	The most important methods making up the core of the Framework rendering
*	Namespace GPU are methods relating directly to the GPU such as memory methods and rendering
*	Namespace Present are methods relating to settings/descriptors for rendering
*	Namespace Pipeline for methods relating to rendering pipeline
*	TODO: Currently the pipeline methods/namespace is very strict, develop a flexible system after tech demo
*	TODO: Currently no debug method to make it easier, add later
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


namespace gpu
{

	struct GpuRange;

	struct GpuHeap;

	struct DeviceDesc;

	struct Device;

	struct Queue;

	struct DeviceCapabilities;

	struct GpuTextureSizeAlign;

	struct SubmitDesc;

	struct Swapchain;

	struct Texture;

	struct CommandBuffer;

	struct CommandPool;

	// In no-api this is under gpu::detail namespace
	struct PresentContext;



	struct DesiredFeatures;

	// Setup
	uint32_t scoreDevice(VkPhysicalDevice device, VkSurfaceKHR surface);
	void pickPhysicalDevice(VkInstance instance, Device* deviceInfo);
	bool createDevice();

	// GPU memory methods
	void* gpuMalloc(size_t bytes);
	void* gpuMalloc(size_t bytes, size_t alignment);
	void* gpuHostToDevicePointer(void* ptr);
	void gpuMemCopy();
	void gpuFree(void* ptr);
	void gpuSetActiveTextureHeapPtr();

	// GPU drawing methods
	void gpuDispatch();
	void gpuDraw();
	void gpuDrawIndexed();

	// Copy methods
	void copyMemory();
	void copyMemoryToTexture();
	void copyTextureToMemory();

	// Render pass methods
	void gpuBeginRenderPass(CommandBuffer* buffer);
	void gpuEndRenderPass();


	void submitCommands(Device* device, const SubmitDesc& desc, uint32_t queue_index); // TODO: Missing semaphores
	void submitAndPresent(Device* device, const SubmitDesc& desc);

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

	void bindPSO(CommandBuffer* commands, const PSO* pso);

	PSO* createRasterPSO(Device* device, const ShaderStage& vertStage, const ShaderStage& fragStage, const RasterizationState& rastState);

	PSO* createGraphicsPSO(Device* device, GraphicsPSODesc graphicsDesc);
} // pipeline

namespace gpu::shader
{

}  // shader

namespace gpu::sync
{
	struct TimelinePoint;
} // sync