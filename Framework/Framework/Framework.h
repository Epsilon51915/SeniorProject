// Framework.h : Include file for standard system include files,
// or project specific include files.

#pragma once

#include <iostream>
// This is needed to tell GLFW we are using Vulkan
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>



// Basic methods for GLFW window creation
// TODO: These need to be organized into some sort of class or namespace
void createWindow();
void mainLoop();
void cleanUp();


GLFWwindow* window;