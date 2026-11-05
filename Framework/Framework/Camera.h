// Camera.h : Sets up a viewpoint within a window
#pragma once

// Mathematics library for graphics
#include <glm/glm.hpp>

// A point in a 3D space
struct position {
	float x = 0, y = 0, z = 0;
};

class Camera {
public:
	// Position and Field of View (fov). 60 degrees (radians), 5 units away from origin.
	position pos{ 0, 0, 5 };
	float fov = 1.047f;

private:
	// Left to right angle + Up and Down angle in radians.
	float pitch = 0;
	float yaw = 0;
};