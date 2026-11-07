// Camera.h : Sets up a viewpoint within a window
#pragma once

// A point in a 3D space
//struct position {
//	float x = 0, y = 0, z = 0;
//};
//
//class Camera {
//public:
	// Position and Field of View (fov). 60 degrees (radians), 5 units? away from origin.
//	position pos{ 0, 0, 5 };
//	float fov = 1.047f;

//private:
	// Left to right angle + Up and Down angle in radians.
//	float pitch = 0;
//	float yaw = 0;
//};
//This was going to be all from scratch, can still, but might check out glm to make vector math easier.

// OpenGL's Math library
#include <glm/glm.hpp> 
// 4x4 matrix library from glm
#include <glm/mat4x4.hpp>

// vec3 -> 3D positions and directions, (Cooridinate system)
// vectors that are needed for cooridinates
class Camera {
public:
	Camera(glm::vec3 position, glm::vec3 direction, glm::vec3 upReference) {

	}


// Accessors / Getters

// Functions
//	void mouseInput(const float& 


// Vertex coordinates as seen from the camera's perspective
// mat4 -> 4x4 transformation matrices, viewMatrix -> transforms all world coordinates into view coordinates
// float, euler angles for mouse input and transformations. No roll cause no inentions for that use but maybe later.
// Mouse input for later development.
private:
	glm::vec3 position;
	glm::vec3 front;
	glm::vec3 up; 
	glm::vec3 right;
	glm::vec3 upReference;

	glm::mat4 viewMatrix;

	float yaw;
	float pitch;

	float moveSpeed;
	float mouseSensitivity;

	void updateCamVecs();
};