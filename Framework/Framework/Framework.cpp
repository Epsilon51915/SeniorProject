// Framework.cpp : Defines the entry point for the application.
//

#include "Framework.h"

using namespace gpu;

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
	Device* device;
	DesiredDevice desiredDevice;

	createDevice(device, desiredDevice, window);

	std::vector<char> vertexShader = pipeline::readShader("vert.spv", "");
	std::vector<char> fragmentShader = pipeline::readShader("frag.spv", "");

	pipeline::GraphicsPSODesc graphicsPSODesc;


	pipeline::PSO* trianglePSO = pipeline::createGraphicsPSO(device, );

	sync::TimelinePoint latestCompletion;

	// create command buffers
	CommandPool* commandPool;

	while (!glfwWindowShouldClose(window))
	{
		glfwPollEvents();
		glfwSwapBuffers(window);
	}
	cleanUp();
}


// Cleanup all GLFW resources and other framework resources
void cleanUp()
{
	glfwDestroyWindow(window);
	glfwTerminate();
}