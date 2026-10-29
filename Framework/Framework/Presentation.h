#pragma once
#include "GPU.h"

/*
*	Swapchain, render views, and other objects related to getting the results of our renders on screen
*/

using namespace gpu;
namespace present
{
	uint32_t frames_in_flight = 3; // TODO: Needs to be a #define somewhere else
	vk::PresentModeKHR presentMode = vk::PresentModeKHR::eFifo;
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

	struct Swapchain
	{
		Device* state = nullptr;
		VkSwapchainKHR handle = VK_NULL_HANDLE;

		VkImage images[1]{}; // TODO: add max_swapchain_images

		RenderView renderViews[1]{}; // TODO: add max_swapchain_images
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

	struct TextureDesc
	{
		uint32_t mipLevels = 1;
		uint32_t layerCount = 1;
	};

	// Stencil, color blending, and other description methods here
	void createBlendState();
	void modifyBlendState();
	void createRenderPassState();
	void modifyRenderPassState();
	void createDepthStencilState();
	void modifyDepthStencilState();
}