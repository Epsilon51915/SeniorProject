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




// enum declarations
enum AATYPE {MSAA, FXAA, NONE};


// Empty handles to be defined somewhere else
struct RenderView; // May also be called render surface

struct Vertex
{
	glm::vec3 position;
	glm::vec3 normal;
	glm::vec3 color;
	glm::vec2 texCoords;
};


// NOTE: Will be changed to be how Vulkan handles the device/logical device, this is a place holder
struct GpuInfo
{
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

struct renderSettings
{
	AATYPE aatype;
	uint32_t anistropicLevel; // Double check levels
};

struct textureSettings
{
	uint8_t mipLevel;
	uint8_t mipCount;
	uint32_t sampleCount;
};

// Memory management
// Nearly 1-1 from Prototype API from noapi blog post by Sebastian Aaltonen
// TODO: missing args listed in blog-post, kept missing for now until we have a better idea on implementation
void* gpuMalloc(size_t bytes);
void* gpuMalloc(size_t bytes, size_t alignment);
void gpuFree(void* ptr);
void* gpuHostToDevicePointer(void* ptr);