/*
*	The most important methods making up the core of the Framework rendering
*	Namespace GPU are methods relating directly to the GPU such as memory methods and rendering
*	Namespace Present are methods relating to settings/descriptors for rendering
*	Namespace Pipeline for methods relating to rendering pipeline
*	TODO: Currently the pipeline methods/namespace is very strict, develop a flexible system after tech demo
*/


#pragma once
#include <vulkan/vulkan_raii.hpp>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>
#include <GLFW/glfw3.h>
#include <cassert>


namespace gpu
{

	static constexpr int width = 1280;
	static constexpr int height = 720;

	static uint32_t frameIndex = 0;
	static const uint32_t maxColorAttachments = 1;
	static vk::PresentModeKHR presentMode = vk::PresentModeKHR::eFifo;
	static GLFWwindow* window;

	const uint32_t maxSwapchainImages = 3;

	struct GpuRange;

	struct GpuHeap;

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

	// bar-for-bar from no-api github
	// TODO: will probably need to be changed in the future
	struct PSO;

	struct DesiredFeatures;

	// Setup
	void pickPhysicalDevice();
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
	void bindPSO();

	void submitCommands(Device* device, const SubmitDesc& desc, uint32_t queue_index); // TODO: Missing semaphores
	void submitAndPresent(Device* device, const SubmitDesc& desc);

	// GPU Queue methods
	void gpuCreateQueue();
	void gpuStartCommandRecording(Queue& queue);
	void gpuSubmitCommandBuffers();
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

} // pipeline

namespace gpu::shader
{

}  // shader

namespace gpu::sync
{
	struct TimelinePoint;
} // sync