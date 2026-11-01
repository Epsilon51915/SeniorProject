#pragma once
#include <vulkan/vulkan_raii.hpp>
#include <vulkan/vulkan.h>
#include <cassert>
#include "Buffers.h"
#include "Presentation.h"

/*
*	The GPU namespace will contain access to all gpu operations
*	Kept in a namespace for organizational reasons as making a GPU class is not necessary
* 
*	Inherited at Presentation.h where rendering information is given to these methods
*	TODO: Methods regarding rendering information may need to be moved to presention or some combination of presen.h and gpu.h
*	TODO: Potentially extend the namespace inside presen.h but it could be gross
*	TODO: Potentially move _Desc into the rendersettings.h so it can be accessed here and wont need to move GPU methods
*	TODO: Lots of cross-referencing, remember to reorganize
*/


namespace gpu
{

	uint32_t frameIndex = 0;
	vk::PresentModeKHR presentMode = vk::PresentModeKHR::eFifo;

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

	// In no-api this is under gpu::detail namespace
	struct PresentContext;

	// bar-for-bar from no-api github
	// TODO: will probably need to be changed in the future
	struct PSO;

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
	void gpuBeginRenderPass(CommandBuffer* bufferr, const present::RenderingDesc& renderDesc);
	void gpuEndRenderPass();
	void bindPSO();

	void submitCommands(Device* device, const SubmitDesc& desc, uint32_t queue_index); // TODO: Missing semaphores
	void submitAndPresent(Device* device, const SubmitDesc& desc);

	// GPU Queue methods
	void gpuCreateQueue();
	void gpuStartCommandRecording(Queue& queue);
	void gpuSubmitCommandBuffers();
} // gpu