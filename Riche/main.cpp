#include <iostream>

#define GLFW_INCLUDE_VULKAN
#include "GLFW/glfw3.h"
#include "glm/glm.hpp"

#include "Graphics/GfxCore.h"

GLFWwindow* window;

int main()
{
	glfwInit();

	// Set GLFW to Not work with OpenGL, For Vulkan
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

	window = glfwCreateWindow(1440, 720, "Hi", nullptr, nullptr);
	GfxDevice device;
	device.Initialize(window);

	while (!glfwWindowShouldClose(window))
	{
		glfwPollEvents();
	}

	device.Destroy();
	// Destroy GLFW window and stop GLFW
	glfwDestroyWindow(window);
	glfwTerminate();
	return 0;
}