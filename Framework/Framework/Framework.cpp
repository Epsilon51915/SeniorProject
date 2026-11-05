// Framework.cpp : Defines the entry point for the application.
//

#include "Framework.h"
#include "Camera.h"
#include "ECS.h"

using namespace std;

//test ecs declaration
void testECS();
//

int main()
{
	// Testing if Camera is there w/no frame of reference
	// Testing ECS w/camera
	Camera camera;
std:cout << "Cam" << camera.pos.x << ", " << camera.pos.y << ", " << camera.pos.z << "\n";
	//
	testECS();
	//
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

//TEST
// Verify entities can be created
// Creates instance of ECS registry
// Tkaes our camera, makes it an entity then outputs its ID for verification
void testECS() {
	registry ecs;
	EntityID cameraEntity = ecs.createEntity();
	std::cout << "CamID: " << cameraEntity << endl;

}
//TEST

// Cleanup all GLFW resources and other framework resources
void cleanUp()
{
	glfwDestroyWindow(window);
	glfwTerminate();
}