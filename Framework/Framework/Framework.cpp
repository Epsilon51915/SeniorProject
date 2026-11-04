// Framework.cpp : Defines the entry point for the application.
//

#include "Framework.h"
#include "Camera.h"

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

	// Testing if Camera is there w/no frame of reference
	Camera camera;
std:cout << "Cam" << camera.pos.x << ", " << camera.pos.y << ", " << camera.pos.z << "\n";
	//

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