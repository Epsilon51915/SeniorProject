// Framework.cpp : Defines the entry point for the application.
//

#include "Framework.h"
#include "Camera.h"
#include "ECS.h"

using namespace gpu;

//test ecs declaration
void testECS() {
	registry ECS;
	EntityID entity = ECS.createEntity();
	std::cout << "Entity ID: " << entity << "\n";
}
//

int main()
{
	// Testing ECS 
	testECS();
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