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



void cleanUp()
{
	glfwDestroyWindow(window);
	glfwTerminate();
}