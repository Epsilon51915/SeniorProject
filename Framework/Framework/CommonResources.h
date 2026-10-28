/*
*	Commonly used resources, APIs, libraries, and variables are stored here
*	These are used across the entirety of the project and kept here for easy organization
*	NOTE: Some of these are only in here because I *think* it might be useful or used project wide
*	NOTE: Many of these will probably be moved/edited/repurposed as the project develops
*	TODO: Some of these need to be seperate into namespaces for better organization
*/

#pragma once

#include <glm/glm.hpp>
#include <string>
#include <assert.h>
#include <vulkan/vulkan_raii.hpp>
#include <vector>




// enum declarations
enum PRESENT_MODE
{
	IMMEDIATE, FIFO, FIFO_RELAXED, MAILBOX
};

enum CULL_MODE {
	NONE, CW, CCW
};

// Empty handles to be defined somewhere else
struct RenderView; // May also be called render surface

struct Device;

struct Vertex
{
	glm::vec3 position;
	glm::vec3 normal;
	glm::vec3 color;
	glm::vec2 texCoords;
};

struct VulkanInfo
{
	vk::Instance instance = nullptr;
	vk::raii::Context context;
};

// NOTE: Will be changed to be how Vulkan handles the device/logical device, this is a place holder
struct GpuInfo
{
	Device* device = nullptr;
	std::string name;
	int totalVRAM;
	// NOTE: These fields may not be used
	int numGraphicsQueues;
	int numComputeQueues;
	int numTransferQueues;
};

struct alignas(16) textureData
{
	uint32_t srcTextureBase;
	uint32_t dstTexture;
	glm::float32 invDimensions; // TODO: noapi blog says this should be float32x2, define later as a seperate struct
};



struct textureSettings
{
	uint8_t mipLevel;
	uint8_t mipCount;
	uint32_t sampleCount;
};

struct Queue
{
	uint32_t familyIndex = 0;
};

struct TextureHeap
{
	uint64_t size = 0;
};

struct CommandBuffer
{
	Queue* queue;
	vk::CommandBuffer commandBuffer;
};

struct CommandPool
{
	vk::CommandPool commandPool;
};

struct RenderView
{

};


struct SamplerDescriptorHeap
{

};

struct Semaphore
{
	vk::Semaphore semaphore;
};

struct ShaderData
{

};

// Memory management
// Nearly 1-1 from Prototype API from noapi blog post by Sebastian Aaltonen
// TODO: missing args listed in blog-post, kept missing for now until we have a better idea on implementation


// Buffer management
void recordBuffer();
void submitCommandPool();


// Vulkan setup methods

// Create VkInstance
void initVulkan();
// Find device and fill in info
void findDevice(GpuInfo& info);
void setupSwapchain();


// Shader management
void loadShader();

// Render Pipeline
namespace rpl
{
	enum AATYPE { MSAA, FXAA, NONE };

	void createRenderPipeline();
	void addPipelineStage();
	void createComputePipeline();

	struct renderSettings
	{
		AATYPE aatype;
		uint32_t anistropicLevel; // TODO: Double check levels
	};

	struct RenderingPipeline
	{
		int stages;
		vk::Pipeline pipeline;
	};

}

