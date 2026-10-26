// Framework.cpp : Defines the entry point for the application.
//

#include "Framework.h"

using namespace std;

int main()
{
	createWindow();
	return 0;
}

void createWindow()
{
	// Init GLFW with Vulkan(GLFW_CLIENT_API, GLFW_NO_API), temp size of 1280, 720 since some of us are on laptops
	glfwInit();
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	window = glfwCreateWindow(1280, 720, "Framework", NULL, NULL);
	mainLoop();
}

void mainLoop()
{
	while (!glfwWindowShouldClose(window))
	{
		glfwSwapBuffers(window);
		glfwPollEvents();
	}
	cleanUp();
}


// Cleanup all GLFW resources and other framework resources
void cleanUp()
{
	glfwDestroyWindow(window);
	glfwTerminate();
}