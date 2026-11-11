// Framework.h : Include file for standard system include files,
// or project specific include files.

#pragma once

#include <iostream>
#include "RenderCore.hpp"



// Basic methods for GLFW window creation
// TODO: These need to be organized into some sort of class or namespace
void createWindow();
void mainLoop();
void cleanUp();


GLFWwindow* window;