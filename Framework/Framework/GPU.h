#pragma once
#include <vulkan/vulkan_raii.hpp>
#include <vulkan/vulkan.h>
#include <cassert>

/*
*	The GPU namespace will contain access to all gpu operations
*	Kept in a namespace for organizational reasons as making a GPU class is not necessary
* 
*	Inherited at Presentation.h where rendering information is given to these methods
*	TODO: Methods regarding rendering information may need to be moved to presention or some combination of presen.h and gpu.h
*	TODO: Potentially extend the namespace inside presen.h but it could be gross
*	TODO: Potentially move _Desc into the rendersettings.h so it can be accessed here and wont need to move GPU methods
*/


namespace gpu
{

	uint32_t frameIndex = 0;
	struct GpuRange;

	struct GpuHeap;

	struct Device;

	struct Queue;

	struct DeviceCapabilities;

	struct GpuTextureSizeAlign;

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
	void gpuBeginRenderPass();
	void gpuEndRenderPass();

	// GPU Queue methods
	void gpuCreateQueue();
	void gpuStartCommandRecording(Queue& queue);
	void gpuSubmitCommandBuffers();
} // gpu