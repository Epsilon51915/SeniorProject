// Framework.cpp : Defines the entry point for the application.
//

#include "Framework.h"

using namespace gpu;

int main()
{
	createWindow();
	mainLoop();
	cleanUp();
	return 0;
}

void createWindow()
{
	// Init GLFW with Vulkan(GLFW_CLIENT_API, GLFW_NO_API), temp size of 1280, 720 since some of us are on laptops
	glfwInit();
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	window = glfwCreateWindow(1280, 720, "Framework", NULL, NULL);
}

void mainLoop()
{
	Device* device;

	DesiredDevice desiredDevice;

	createDevice(device, desiredDevice, window);

	std::vector<char> vertexShader = readShader("vert.spv", "");
	std::vector<char> fragmentShader = readShader("frag.spv", "");

	GraphicsPSODesc graphicsPSODesc;
	graphicsPSODesc.vertex.code = vertexShader;
	graphicsPSODesc.vertex.entryPoint = "vertMain";

	graphicsPSODesc.fragment.code = fragmentShader;
	graphicsPSODesc.fragment.entryPoint = "fragMain";
	

	PSO* trianglePSO = createGraphicsPSO(device, graphicsPSODesc);

	TimelinePoint latestCompletion{ .semaphore = createTimelineSemaphore(device) };

	// create command buffers
	CommandPool* commandPools[] = { createCommandPool(device), createCommandPool(device) };

	while (!glfwWindowShouldClose(window))
	{
		glfwPollEvents();
		CommandPool* commandPool = commandPools[latestCompletion.value % 2];
		resetCommandPool(commandPool);

		CommandBuffer* commands = beginCommands(commandPool);
		const SwapchainFrame frame = acquire(commands);

		

		gpuBeginRenderPass(commands, {
			.colors = { { .renderView = frame.renderView, .load = LoadOp::clear } },
			});

		bindPSO(commands, trianglePSO);
		draw(commands, {}, 3);
		gpuEndRenderPass(commands);
		endCommands(commands);

		latestCompletion.value++;
		submitAndPresent(device, { .commands = { commands }, .completion = latestCompletion });

		//glfwSwapBuffers(window);
	}

	waitIdle(device);

	destroyCommandPool(commandPools[1]);
	destroyCommandPool(commandPools[0]);
	destroyTimelineSemaphore(latestCompletion.semaphore);
	destroyPso(trianglePSO);

	destroyDevice(device);
}


// Cleanup all GLFW resources and other framework resources
void cleanUp()
{
	glfwDestroyWindow(window);
	glfwTerminate();
}