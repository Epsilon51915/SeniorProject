#pragma once

#include <cstdint>
#include "GPU.h"


using namespace gpu;
struct CommandBuffer
{
	Device* state = nullptr;
	CommandBuffer* nextBuffer = nullptr;
	VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
	VkCommandBuffer epilogue = VK_NULL_HANDLE;
	VkQueryPool timestampPool = VK_NULL_HANDLE;

	bool submitted = false;
};

struct CommandPool
{
	Device* state = nullptr;
	VkCommandPool commandPool = VK_NULL_HANDLE;
	CommandBuffer* fistBuffer = nullptr;
	CommandBuffer* lastBuffer = nullptr;
	CommandBuffer* nextBuffer = nullptr;
	bool timeStaps = false;
};