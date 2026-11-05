#pragma once
// Include GLEW and GLFW
#include "glew/glew.h"
#include "glfw/glfw3.h"

// Globals are only declared here, for their initial values go to globals.cpp
// If you want to make it cleaner try to upgrade the cpp version to 17 or later and use a namespace

// Window
extern GLFWwindow* window;
extern int windowWidth;
extern int windowHeight;

// Transformation scales
extern float scaleX;
extern float scaleY;
extern float scaleZ;