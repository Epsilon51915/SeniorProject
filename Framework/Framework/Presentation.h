#pragma once
#include "GPU.h"
#include "Buffers.h"

/*
*	Swapchain, render views, and other objects related to getting the results of our renders on screen
*/

using namespace gpu;
namespace present
{
	uint32_t frames_in_flight = 3; // TODO: Needs to be a #define somewhere else


	struct ClearColor
	{
		float r = 0.0f;
		float g = 0.0f;
		float b = 0.0f;
		float a = 0.0f;
	};

	struct RenderView
	{
		Device* state = nullptr;
		VkImageView view = VK_NULL_HANDLE;
		bool swapchain_view = false;
	};


	struct Stencil
	{

	};

	struct DepthStencilDesc
	{

	};

	struct BlendDesc
	{

	};

	struct RasterizerDesc
	{

	};

	struct RenderPassDesc
	{

	};
	
	struct RenderingDesc
	{

	};

	struct TextureDesc
	{
		uint32_t mipLevels = 1;
		uint32_t layerCount = 1;
	};


	// Render View methods
	RenderView* createRenderView();
	void destroyRenderView();

	// Stencil, color blending, and other description methods here
	void createBlendState();
	void modifyBlendState();
	void createRenderPassState();
	void modifyRenderPassState();
	void createDepthStencilState();
	void modifyDepthStencilState();
}