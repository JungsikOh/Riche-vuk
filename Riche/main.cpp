#include "Rendering/VulkanRenderer.h"

GLFWwindow* window;
VulkanRenderer vulkanRenderer;

double lastX = 0.0, lastY = 0.0;  // ���� �������� ���콺 ��ǥ
bool firstMouse = true;  // �ʱ� ���콺 ��ġ Ȯ���� ���� �÷���
bool rightButtonPressed = false;

void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	// Ű�� ������ ��
	if (action == GLFW_PRESS) {
		std::cout << "Key Pressed: " << key << std::endl;
	}
	// Ű�� ������� ��
	else if (action == GLFW_RELEASE) {
		std::cout << "Key Released: " << key << std::endl;
	}
}

void MouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
	if (button == GLFW_MOUSE_BUTTON_RIGHT) { // ���콺 ������ ��ư�� ���
		if (action == GLFW_PRESS) { // ���� ��
			rightButtonPressed = true;
			std::cout << "Right Mouse Button Pressed" << std::endl;
		}
		else if (action == GLFW_RELEASE) { // �� ��
			rightButtonPressed = false;
			std::cout << "Right Mouse Button Released" << std::endl;
		}
	}
}

void CursorPositionCallback(GLFWwindow* window, double xpos, double ypos) 
{
	if (rightButtonPressed)
	{
		if (firstMouse) {
			// ó���� ���� ��ġ�� ���� ������ �ʱ� ��ġ ����
			lastX = xpos;
			lastY = ypos;
			firstMouse = false;
		}

		// DeltaX, DeltaY ���
		double deltaX = xpos - lastX;
		double deltaY = ypos - lastY;

		// ���� ��ġ�� ���� ��ġ�� ����
		lastX = xpos;
		lastY = ypos;

		// DeltaX�� DeltaY ���
		std::cout << "DeltaX: " << deltaX << ", DeltaY: " << deltaY << std::endl;
	}
}

void InitWindow(std::string wName = "Test Window", const int w = 800, const int h = 600)
{
	// Initialize GLFW
	glfwInit();

	// Set GLFW to Not work with OpenGL, For Vulkan
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

	window = glfwCreateWindow(w, h, wName.c_str(), nullptr, nullptr);
}

int main()
{
	// Create Window
	InitWindow("Test Widnow", 1048, 624);

	// Create Vulkan Renderer Instance
	if (vulkanRenderer.Initialize(window) == EXIT_FAILURE)
	{
		return EXIT_FAILURE;
	}

	float angle = 0.0f;
	float deltaTime = 0.0f;
	float lastTime = 0.0f;

	glfwSetKeyCallback(window, KeyCallback);
	glfwSetMouseButtonCallback(window, MouseButtonCallback);
	glfwSetCursorPosCallback(window, CursorPositionCallback);

	// Loop until closed
	while (!glfwWindowShouldClose(window))
	{
		glfwPollEvents();

		float now = glfwGetTime();
		deltaTime = now - lastTime;
		lastTime = now;

		angle += 10.0f * deltaTime;
		if (angle > 360.0f)
		{
			angle -= 360.0f;
		}

		glm::mat4 firstModel(1.0f);
		glm::mat4 secondModel(1.0f);

		firstModel = glm::translate(firstModel, glm::vec3(-2.0f, 0.0f, -1.0f));
		firstModel = glm::rotate(firstModel, glm::radians(angle), glm::vec3(0.0f, 0.0f, 1.0f));

		secondModel = glm::translate(secondModel, glm::vec3(-1.0f, 0.0f, -1.0f));
		secondModel = glm::rotate(secondModel, glm::radians(angle * 50), glm::vec3(0.0f, 0.0f, 1.0f));

		vulkanRenderer.UpdateModel(0, firstModel);

		vulkanRenderer.Draw();
	}

	vulkanRenderer.Cleanup();

	// Destroy GLFW window and stop GLFW
	glfwDestroyWindow(window);
	glfwTerminate();

	return 0;
}