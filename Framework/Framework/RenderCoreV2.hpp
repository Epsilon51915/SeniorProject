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
#include <glm/glm.hpp>
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

	enum class BlendFactor : uint8_t
	{
		zero,
		one,
		source_color,
		one_minus_source_color,
		destination_color,
		one_minus_destination_color,
		source_alpha,
		one_minus_source_alpha,
		destination_alpha,
		one_minus_destination_alpha,
		source_alpha_saturate,
	};

	enum class BlendOp : uint8_t
	{
		add,
		subtract,
		reverse_subtract,
		minimum,
		maximum,
	};

	enum class CullMode : uint8_t
	{
		none,
		clockwise,
		counter_clockwise,
	};

	struct uint32x3
	{
		uint32_t x = 0;
		uint32_t y = 0;
		uint32_t z = 0;
	};

	struct uint32x2
	{
		uint32_t x = 0;
		uint32_t y = 0;
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
	struct Vertex
	{
		float x = 0.0f;
		float y = 0.0f;
		float z = 0.0f;
		float w = 0.0f;
	};
	struct TexCoords
	{
		float x = 0.0f;
		float y = 0.0f;
	};
	struct Cube
	{
		Vertex vertices;
		TexCoords uv;
	};

	struct TextureManager
	{

	};

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
		const char* entryPoint = nullptr;
		const char* stageName = nullptr;
		std::vector<char> code;
	};
	struct CommandPool;

	struct Candidate;

	struct Viewport
	{
		float x = 0.0f;
		float y = 0.0f;
		float width = 1.0f;
		float height = 1.0f;
		float minDepth = 0.0f;
		float maxDepth = 1.0f;
	};
	struct Scissor
	{
		int32_t x = 0;
		int32_t y = 0;
		uint32_t width = 1;
		uint32_t height = 1;
	};

	struct BlendComponentState
	{
		BlendFactor source = BlendFactor::one;
		BlendFactor destination = BlendFactor::zero;
		BlendOp operation = BlendOp::add;
	};

	struct BlendState
	{
		bool enabled = false;
		BlendComponentState color = {};
		BlendComponentState alpha = {};
	};

	struct ColorTargetDesc
	{
		Format format = Format::undefined;
		BlendState blend = {};
		uint8_t writeMask = 0xf;
	};

	struct ClearColor
	{
		float r = 0.0f;
		float g = 0.0f;
		float b = 0.0f;
		float a = 0.0f;
	};

	struct ColorAttachment
	{
		RenderView* renderView = nullptr;
		LoadOp load = LoadOp::load;
		StoreOp store = StoreOp::store;
		ClearColor clear = {};
	};

	struct StencilFaceState
	{
		CompareOp compare = CompareOp::always;
		StencilOp fail = StencilOp::keep;
		StencilOp pass = StencilOp::keep;
		StencilOp depth_fail = StencilOp::keep;
		uint8_t reference = 0;
	};

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

	struct RenderingDesc
	{
		std::vector<ColorAttachment> colors;
		DepthAttachment depth = {};
		StencilAttachment stencil = {};
	};

	struct RasterizationState
	{
		CullMode cull = CullMode::none;
		float depthBiasConstant = 0.0f;
		float depthBiasClamp = 0.0f;
		float depthBiasSlope = 0.0f;
	};

	struct GraphicsPSODesc
	{
		ShaderStage vertex = {};
		ShaderStage fragment = {};
		std::vector<ColorTargetDesc> colorTargets = {};
		Format depthFormat = Format::undefined;
		Format stencilFormat = Format::undefined;
		RasterizationState rasterization = {};
	};

	struct SubmitDesc
	{
		std::vector<CommandBuffer*> commands;
		std::vector<TimelinePoint> waits;
		TimelinePoint completion = {};
	};

	struct SwapchainFrame
	{
		RenderView* renderView = nullptr;
		uint32x2 extent = {};
	};

	// Theory on this:
	// To improve tracking of create resources related to Vulkan but NOT the ECS system, we can register a created object by pointer here
	// Then to delete said object with delete[], we can send this struct into a function to get rid of it
	// So then in main or wherever we want to track data, create a ResourceTracker for a type of resource like textures, then when we are done with ALL texture, delete them?
	template <typename T>
	struct ResourceTracker
	{
		size_t size;
		T* dataStart;
	};

	/// Forward decleration of most methods to be defined in RenderCore.cpp
	// NOTE: maintenanceExtensionName currently isn't used, TODO: add safety check maybe to swapchain maintenance check
	VkResult inspectDevice(VkPhysicalDevice device, const DesiredDevice& desc, const char* maintenanceExtensionName, VkSurfaceKHR surface, Candidate& output, bool& bareMinFeatures);
	bool hasName(const std::vector<VkExtensionProperties>& extensionArr, const char* nameCheck);
	bool hasName(const std::vector<const char*>& extensionArr, const char* nameCheck);
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

	void setViewport(CommandBuffer* commands, const Viewport& viewport);
	void setScissor(CommandBuffer* commands, const Scissor& scissor);
	void setDepthStencil(CommandBuffer* commands, const DepthStencilState& state);

	// Temp texture methods to be implmented in the texture class later
	uint64_t calcTextureByteSize(uint32_t width, uint32_t height);

	void matrixMult(glm::mat4& currentMatrix, glm::mat4& mult);

	void bindIndices(CommandBuffer* buffer, uint32_t* indices, Vertex* vertices);
	void bindIndices3KHR();

	void gpuBeginRenderPass(CommandBuffer* buffer, const RenderingDesc& desc, RenderingFlags flags = RenderingFlags::none);
	void gpuEndRenderPass(CommandBuffer* buffer);
	void draw(CommandBuffer* commands, const void* root, uint32_t vertexCount, uint32_t instanceCount = 1, uint32_t firstVertex = 0,
		uint32_t firstInstance = 0);
	void drawIndexed(CommandBuffer* commands, const void* root, uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance);
	void submitCommands(Device* device, const SubmitDesc& desc, uint32_t queue_index, VkSemaphore wait_semaphore, VkSemaphore signal_semaphore);
	void submitAndPresent(Device* device, const SubmitDesc& desc);
	void emitRootPointer(CommandBuffer* commands, const void* root);

	SwapchainFrame acquire(CommandBuffer* commands);
	bool swapchainSurfaceConfigurationChanged(const Swapchain& swapchain);

	CommandPool* createCommandPool(Device* device, uint32_t queueIndex = 0);
	void resetCommandPool(CommandPool* pool);
	CommandBuffer* beginCommands(CommandPool* pool);
	void recordImageBarriers(VkCommandBuffer commandBuffer, const VkImageMemoryBarrier2* barriers, uint32_t numBarriers);
	void endCommands(CommandBuffer* buffer);

	TimelineSemaphore* createTimelineSemaphore(Device* device, uint64_t initial_value = 0);
	void waitTimeline(TimelinePoint point);
	void waitIdle(Device* device);

	/// Shader methods
	// Refer to orignial render core for my little theory on this
	// Take an array of the container and dynamically create a new pipeline
	PSO* createCustomPipeline(CustomPipelineContainer& container);
	PSO* createGraphicsPSO(Device* device, GraphicsPSODesc graphicsDesc);
	void bindPSO(CommandBuffer* commands, const PSO* pso);
	std::vector<char> readShader(const char* fileName, const char* filePath);

	/// Memory Management methods
	template <typename T>
	void deleteObject(T* data, uint32_t dataSize = 1);
	void destroyOwnedSwapchain(Device& device) noexcept;
	void destroyPso(PSO* pso);
	void destroyCommandPool(CommandPool* pool);
	void destroyTimelineSemaphore(TimelineSemaphore* semaphore);
	void destroyDevice(Device* device);

}