#pragma once
#include "GPU.h"

/*
*	Swapchain, render views, and other objects related to getting the results of our renders on screen
*/

using namespace gpu;
namespace present
{
	uint32_t frames_in_flight = 3;
	vk::PresentModeKHR presentMode = vk::PresentModeKHR::eFifo;

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
}