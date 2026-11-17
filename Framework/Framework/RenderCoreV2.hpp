#pragma once

/*
*	A revised render core file to improve on the disorganization of the original, and fit it better to our project goals.
*	Focus will be on useability, better automation of certain tasks for the user, and generally better documention.
*	This will also see us (attempt) to implement a custom render pipeline
*	Unlike in noapi, it is assumed we want to present something. As such, any checks for an existing surface, or presentation value, have been removed or replaced
*	TODO: Create a documentation.readme so we can begin cataloging new changes/features for future reference
*	TODO: Properly apply inline/unnamed namespaces: https://www.learncpp.com/cpp-tutorial/unnamed-and-inline-namespaces/
*   TODO: Add bufferMap{} variable to device struct, and initialize it in createDevice()
*/

#define VULKAN_HPP_NO_CONSTRUCTORS
#include <vulkan/vulkan.h>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <iostream>
#include <string>
#include <cstdint>
#include <vector>
#include <stdexcept>
#include <fstream>
#include <stddef.h>
#include <stdio.h>

namespace core
{
	/// Important enums for program management
	enum class MemoryType : uint8_t
	{
		cpu_visible,
		gpu_only,
		readback,
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
	// NOTE: Not sure if this is needed. Add either more options or remove it
	enum class ColorSpace : uint32_t { srgb, extended_srgb_linear };

	struct uint32x3
	{
		uint32_t x = 0;
		uint32_t y = 0;
		uint32_t z = 0;
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
	struct TimelineSemaphore
	{
		Device* state = nullptr;
		VkSemaphore semaphore = VK_NULL_HANDLE;
	};
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

		ColorSpace swapchainColorSpace = ColorSpace::srgb;
		Format swapchainFormat = Format::undefined;
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

	struct Candidate;

	/// Forward decleration of most methods to be defined in RenderCore.cpp
	// NOTE: maintenanceExtensionName currently isn't used, TODO: add safety check maybe to swapchain maintenance check
	VkResult inspectDevice(VkPhysicalDevice device, const DesiredDevice& desc, const char* maintenanceExtensionName, VkSurfaceKHR surface, Candidate& output);
	bool hasName(const std::vector<VkExtensionProperties>& extensionArr, const char* nameCheck);
	VkResult getInstanceExtensions(std::vector<VkExtensionProperties>& extensions);
	VkResult getDeviceInstanceExtensions(std::vector<VkExtensionProperties>& extensions, VkPhysicalDevice device);
	VkResult getInstanceLayerProperties(std::vector<VkLayerProperties>& extensions);
	// Find physical device to be used and create the Vulkan logical device
	DeviceInit createDevice(const DesiredDevice desc);
	// Create the debug method callback IF debug compilation mode
	VkResult createDebugUtil();
	//static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
	//	VkDebugUtilsMessageTypeFlagsEXT messageType,
	//	const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
	//	void* pUserData);

	/// Shader methods
	// Refer to orignial render core for my little theory on this
	// Take an array of the container and dynamically create a new pipeline
	PSO* createCustomPipeline(CustomPipelineContainer& container);
	PSO* createStandardPipeline();
	const char* readShader(const char* fileName, const char* filePath);

	/// Memory Management methods
	template <typename T>
	void deleteObject(T* data, uint32_t dataSize = 1);
	void destroyOwnedSwapchain(Device& device) noexcept;
	void destroyPso(PSO* pso);
	void destroyCommandPool(CommandPool* pool);
	void destroyTimelineSemaphore(TimelineSemaphore* semaphore);
	void destroyDevice(Device* device);

}