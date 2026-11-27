// Framework.cpp : Defines the entry point for the application.
//

#include "Framework.h"
#include "Camera.h"
#include "ECS.h"

using namespace std;

//test ecs declaration
void testECS();
//

using namespace core;

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

TEST_CASE("Testing ECS")
{
	//CHECK(createEntity() == 1);
	//CHECK(createEntity() == 2);
	registry testRegistry;

	EntityID e1 = testRegistry.createEntity(); // Create entity

	CHECK(testRegistry.exists(e1)); // Check does entity exists

	testRegistry.delEntity(e1); // Delete entity

	CHECK_FALSE(testRegistry.exists(e1)); // Verify entity was deleted

	// Test for children entities
	EntityID parent = testRegistry.createEntity();// Create parent
	EntityID child1 = testRegistry.createEntity();// Create child1, child2
	EntityID child2 = testRegistry.createEntity();
	EntityID random1 = testRegistry.createEntity(); // Randoms not supposed to be children
	EntityID random2 = testRegistry.createEntity();

	testRegistry.setParent(parent, child1); // Set the relationship
	testRegistry.setParent(parent, child2);

	std::vector<EntityID> children = testRegistry.getChildren(parent); // Get all ID's of the children associated with a parent and store them.

	CHECK(testRegistry.getParent(child1) == parent); // Check if child's stored parent ID is same as assigned parent ID
	CHECK(child1 != parent); // Check that child ID is not same as parent ID
	CHECK(children.size() == 2); // Check if two child entites are aassoicated with a parent

}

int main(int argc, char** argv) {
	doctest::Context context;

	//// !!! THIS IS JUST AN EXAMPLE SHOWING HOW DEFAULTS/OVERRIDES ARE SET !!!

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
	
	mainLoop();
	cleanUp();
	return res + client_stuff_return_code; // the result from doctest is propagated here as well
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