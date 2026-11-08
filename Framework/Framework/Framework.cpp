// Framework.cpp : Defines the entry point for the application.
//

#include "Framework.h"
#include "Camera.h"
#include "ECS.h"

using namespace gpu;

// Global variables for mouse handling
// Forward declarations
Camera camera;
float lastX = 0.0f, lastY = 0.0f;
bool firstMouse = true;
void mouseCallback(GLFWwindow* window, double xpos, double ypos);

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
	// Set up input callbacks and captures the cursor for camera controls
	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
	glfwSetCursorPosCallback(window, mouseCallback);
	//TODO: glfwSetScrollCallback(window, scrollCallback);

	mainLoop();
}

void mainLoop()
{
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
		//
		//glfwSwapBuffers(window);
	}
	cleanUp();
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