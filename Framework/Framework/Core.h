#pragma once
#include <vulkan/vulkan_raii.hpp>
#include <cstdint>
#include <GLFW/glfw3.h>
#include <stdexcept>
#include <iostream>


/*
*	Where the Vulkan context/instance will exist
*	Where command from <insert headers here> are integrated and used
*/




void initVulkan();
void findDevice();
void createSwapchain();

// variables to be sorted in a later change
vk::raii::Context vkContext;
vk::raii::Instance vkInstance = nullptr;