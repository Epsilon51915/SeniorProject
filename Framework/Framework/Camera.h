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

// Camera implementation reference:
// https://www.youtube.com/watch?v=JcLHExXet3w&t=602s Suraj Sharma - OpenGL / C++ 3D Tutorial 39 - Camera class (PART 1)
// https://docs.vulkan.org/tutorial/latest/Building_a_Simple_Engine/Camera_Transformations/04_camera_implementation.htmlVulkan Documentation - Camera & Transformations: Camera Implementation
// GLM - OpenGL Mathematics, Liscensed under the MIT liscense
// https://github.com/g-truc/glm
// Khronos Vulkan Tutorial

// OpenGL's Math library
#include <glm/glm.hpp> 
// 4x4 matrix library from glm
#include <glm/mat4x4.hpp>
// glm::translate, rotate, scale
#include <glm/ext/matrix_transform.hpp>
// glm::perspective
#include <glm/ext/matrix_clip_space.hpp>

// vec3 -> 3D positions and directions, (Cooridinate system)
// vectors that are needed for cooridinates, upReference -> relative to the world
class Camera {
public:
	// Start at 5 units away from origin so able to see objects when initialized
	// Y-axis is up
	// Look along negative Z-axis (OpenGl convention)
	// Level horizon
	// upReference -> used to build right and up, fixed from "world"
	Camera(
		glm::vec3 position = glm::vec3(0.0f, 0.0f, 5.0f),
		glm::vec3 upReference = glm::vec3(0.0f, 1.0f, 0.0f),
		float yaw = -90.0f,
		float pitch = 0.0f
	) : position(position), upReference(upReference), yaw(yaw), pitch(pitch)
	{
		updateCamVecs();
	}
// Accessors / Getters
 	// mat4 -> 4x4 transformation matrices
	// Matrix projection for graphics pipeline, ViewMatrix -> Determines where camera is and direction, ProjectionMatrix -> How the 3D world is projected on a 2D screen
	// Transforms world coords to view coords for camera
	glm::mat4 getViewMatrix() const
	{
		return glm::lookAt(position, position + front, up);
	}
	// Transforms the view coords into clip coords
	glm::mat4 getProjectionMatrix(float aspectRatio, float nearPlane = 0.1f, float farPlane = 100.0f) const
	{
		return glm::perspective(glm::radians(zoom), aspectRatio, nearPlane, farPlane);
	}

	// Tests for camera, since nothing draws yet check with numbers
	// Getting direction and position
	const glm::vec3& getFront() const 
	{ 
		return front; 
	}
	const glm::vec3& getPosition() const 
	{ 
		return position; 
	}
	// 

// Functions
	// Use the mouse to adjust yaw and pitch angles
	// Constrain pitch to avoid camera flipping
	void processMouseMovement(float xOffset, float yOffset, bool constrainPitch = true)
	{
		xOffset *= mouseSensitivity;
		yOffset *= mouseSensitivity;
		yaw += xOffset;
		pitch += yOffset;

		if (constrainPitch)
		{
			pitch = glm::clamp(pitch, -89.0f, 89.0f);
		}
		updateCamVecs();
	}

// Vertex coordinates as seen from the camera's perspective
// float, euler angles for mouse input and transformations. No roll cause no inentions for that use but maybe later.
// Mouse input for later development.
private:
	glm::vec3 position;
	glm::vec3 front;
	glm::vec3 up; 
	glm::vec3 right;
	glm::vec3 upReference;

	// moved to public
	//glm::mat4 getViewMatrix() const;
	//glm::mat4 getProjectionMatrix(float aspectRatio, float nearPlane = 0.1f, float farPlane = 100.0f) const;
	//

	float yaw;
	float pitch;

	float moveSpeed = 3.0f;
	float mouseSensitivity = 0.1f;
	float zoom = 60.0f; // Field of View (fov). 60 degrees 

	void updateCamVecs() 
	{
		// Calculate new front vector as well as the right and up vectors
		glm::vec3 newFront;
		newFront.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
		newFront.y = sin(glm::radians(pitch));
		newFront.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
		front = glm::normalize(newFront);
		right = glm::normalize(glm::cross(front, upReference));
		up = glm::normalize(glm::cross(right, front));
	}
};

// TODO: Place above class for when functions need to call in processKeyboard
// Defining an Enum class for future development of camera directions 
enum class CameraMovement
{
	FORWARD,
	LEFT,
	RIGHT,
	BACKWARD,
	UP,
	DOWN
};