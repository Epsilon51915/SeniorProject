#pragma once

/*
*	A revised render core file to improve on the disorganization of the original, and fit it better to our project goals.
*	Focus will be on useability, better automation of certain tasks for the user, and generally better documention.
*	This will also see us (attempt) to implement a custom render pipeline
*	Unlike in noapi, it is assumed we want to present something.
*	TODO: Create a documentation.readme so we can begin cataloging new changes/features for future reference
*	TODO: Properly apply inline/unnamed namespaces: https://www.learncpp.com/cpp-tutorial/unnamed-and-inline-namespaces/
*/

#include <vulkan/vulkan.h>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <iostream>
#include <cstdint>

namespace gpu
{
	/// Important enums for program management
	enum class MemoryType : uint8_t
	{
		cpu_visible,
		gpu_only,
		readback,
	};


	/// Foward decleration of structs used outside of header ONLY as pointers
	struct Device;
	struct Swapchain;
	struct RenderView;
	struct Queue;
	struct CustomShaderStage;

	struct CommandBuffer;
	struct CommandPool;
	struct PSO;
	struct Texture;

	/// Structs to be used without pointers outside of file
	struct PresentContext
	{
		VkSemaphore acquired = VK_NULL_HANDLE;
		VkSemaphore rendered = VK_NULL_HANDLE;
		VkFence presented = VK_NULL_HANDLE;
		VkSwapchainKHR swapchain = VK_NULL_HANDLE;
		bool presentPending = false;
	};
	struct CustomPipelineContainer
	{
		CustomShaderStage* stages = nullptr;
		CustomShaderStage* tail = nullptr;
		uint32_t stageCount = 0;
	};
	struct RenderSettings
	{
		// Desired Swapchain settings
		VkFormat desiredColorSpace;
		VkFormat desiredImageFormat;

		uint32_t mipLevel = 0;
		uint32_t AALevel = 2;

		// Culling settings
		VkCullModeFlagBits cullMode = VK_CULL_MODE_BACK_BIT;
	};
	struct DesiredDevice
	{
		GLFWwindow* window = nullptr;
		uint32_t desiredGraphicsQueues = 1; // Graphics(present) must be at least 1
		uint32_t desiredComputeQueues = 0;
		uint32_t desiredTransferQueues = 0;

		uint32_t desiredSwapchainImageCount = 2;
		uint32_t timestampQueryCount = 256;

		//VkColorSpaceKHR colorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
		//Format swapchain_format = Format::undefined;
	};
	struct DeviceInit
	{
		Device* newDevice = nullptr;
		VkResult errorCode = VK_SUCCESS;
	};
	struct Viewport;
	struct Scissor;
	struct ShaderStage
	{
		const char* shaderCode;
		const char* entryPoint;
		const char* stageName;
	};
	struct CommandPool;

	// Filled in when inspecting potential devices
	struct Candidate
	{

	};

	/// Forward decleration of most methods to be defined in RenderCore.cpp
	void getQueueInfo(VkPhysicalDevice device);
	void inspectDevice(VkPhysicalDevice device);
	// Find physical device to be used and create the Vulkan logical device
	DeviceInit createDevice();
	// Create the debug method callback IF debug compilation mode
	VkResult createDebugUtil();
	static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
		VkDebugUtilsMessageTypeFlagsEXT messageType,
		const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
		void* pUserData);

	/// Shader methods
	// Refer to orignial render core for my little theory on this
	// Take an array of the container and dynamically create a new pipeline
	PSO* createCustomPipeline(CustomPipelineContainer& container);
	PSO* createStandardPipeline();
	const char* readShader(const char* fileName, const char* filePath);

	/// Memory Management methods
	template <typename T>
	void deleteObject(T* data, uint32_t dataSize = 1);
	void cleanUp();

}