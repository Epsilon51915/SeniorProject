// Framework.h : Include file for standard system include files,
// or project specific include files.

#pragma once

#include <iostream>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>




void createWindow();
void mainLoop();
void cleanUp();


GLFWwindow* window;