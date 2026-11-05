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


struct Vertex
{
	glm::vec3 position;
	glm::vec3 normal;
	glm::vec3 color;
	glm::vec2 texCoords;
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
