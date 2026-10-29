#pragma once
#include <vulkan/vulkan_raii.hpp>
#include <vulkan/vulkan.h>

/*
*	The GPU namespace will contain access to all gpu operations
*	Kept in a namespace for organizational reasons as making a GPU class is not necessary
*/


namespace gpu
{
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
	};

	struct GpuTextureSizeAlign
	{
		size_t size;
		size_t align;
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
}