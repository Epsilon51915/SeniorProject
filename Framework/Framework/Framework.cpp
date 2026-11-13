// Framework.cpp : Defines the entry point for the application.
//
#define DOCTEST_CONFIG_IMPLEMENT
#include "Framework.h"
#include "Camera.h"
#include "ECS.h"
#include <doctest/doctest.h>

using namespace gpu;

// Global variables for mouse handling
// Forward declarations
Camera camera;
registry ECS;
float lastX = 0.0f, lastY = 0.0f;
bool firstMouse = true;
void mouseCallback(GLFWwindow* window, double xpos, double ypos);

// Create an entity
EntityID createEntity() {
	EntityID entity = ECS.createEntity();
	//std::cout << "Entity ID: " << entity << "\n";
	return entity;
}
//

//TEST_CASE("Testing ECS")
//{
//	CHECK(createEntity() == 1);
//	CHECK(createEntity() == 2);
//}

int main(int argc, char** argv) {
	doctest::Context context;

	// !!! THIS IS JUST AN EXAMPLE SHOWING HOW DEFAULTS/OVERRIDES ARE SET !!!

	// defaults
	context.addFilter("test-case-exclude", "*math*"); // exclude test cases with "math" in their name
	context.setOption("abort-after", 5);              // stop test execution after 5 failed assertions
	context.setOption("order-by", "name");            // sort the test cases by their name

	context.applyCommandLine(argc, argv);

	// overrides
	context.setOption("no-breaks", true); // don't break in the debugger when assertions fail

	int res = context.run(); // run

	if (context.shouldExit()) // important - query flags (and --exit) rely on the user doing this
		return res;           // propagate the result of the tests

	int client_stuff_return_code = 0;
	// your program - if the testing framework is integrated in your production code

	if (res == 0)
	{
		createWindow();
	}
	
	//createWindow();
	mainLoop();
	cleanUp();
	return res + client_stuff_return_code; // the result from doctest is propagated here as well
	//return 0;
}

void createWindow()
{
	// Init GLFW with Vulkan(GLFW_CLIENT_API, GLFW_NO_API), temp size of 1280, 720 since some of us are on laptops
	glfwInit();
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	window = glfwCreateWindow(1280, 720, "Framework", NULL, NULL);
	// Set up input callbacks and captures the cursor for camera controls
	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
	glfwSetCursorPosCallback(window, mouseCallback);
	//TODO: glfwSetScrollCallback(window, scrollCallback);

	mainLoop();
}

void mainLoop()
{
	int loop = 0;

	Device* device = create_device({ .window = window, .swapchain_format = Format::bgra8_srgb }).device;

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
		// Temp: Testing camera
		glm::vec3 p = camera.getPosition();
		glm::vec3 f = camera.getFront();
		printf("Position: %.2f, %.2f, %.2f | Front:  %.2f, %.2f, %.2f \n", p.x, p.y, p.z, f.x, f.y, f.z);

		//Esc to quit now since cursor is now captured for camera
		if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
		{
			glfwSetWindowShouldClose(window, true);
		}

		if (latestCompletion.value >= 2)
			wait_timeline({ .semaphore = latestCompletion.semaphore, .value = latestCompletion.value - 1 });
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

// Handle mouse movements for camera rotation
//Manages state, persistence and coordinate transformations for rotation control
void mouseCallback(GLFWwindow* window, double xpos, double ypos)
{
	if (firstMouse)
	{
		lastX = xpos;
		lastY = ypos;
		firstMouse = false;
	}
	float xoffset = xpos - lastX;
	float yoffset = lastY - ypos; // Y ranges bottom to top so its reversed

	lastX = xpos;
	lastY = ypos;

	camera.processMouseMovement(xoffset, yoffset);
}
// Handles the scrollwheel on the mouse
// void scrollCallback(GLFWwindow* window, double xoffset, double yoffset)
//{
	// TODO: camera.processMouseScroll(yoffset);
//}