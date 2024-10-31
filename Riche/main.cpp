#include "Rendering/VulkanRenderer.h"

GLFWwindow* window;
VulkanRenderer vulkanRenderer;

double lastX = 0.0, lastY = 0.0;  // 이전 프레임의 마우스 좌표
bool firstMouse = true;  // 초기 마우스 위치 확인을 위한 플래그
bool rightButtonPressed = false;

void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	// 키가 눌렸을 때
	if (action == GLFW_PRESS) {
		std::cout << "Key Pressed: " << key << std::endl;
	}
	// 키가 릴리즈될 때
	else if (action == GLFW_RELEASE) {
		std::cout << "Key Released: " << key << std::endl;
	}
}

void MouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
	if (button == GLFW_MOUSE_BUTTON_RIGHT) { // 마우스 오른쪽 버튼일 경우
		if (action == GLFW_PRESS) { // 누를 때
			rightButtonPressed = true;
			std::cout << "Right Mouse Button Pressed" << std::endl;
		}
		else if (action == GLFW_RELEASE) { // 뗄 때
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
			// 처음엔 이전 위치가 없기 때문에 초기 위치 설정
			lastX = xpos;
			lastY = ypos;
			firstMouse = false;
		}

		// DeltaX, DeltaY 계산
		double deltaX = xpos - lastX;
		double deltaY = ypos - lastY;

		// 현재 위치를 이전 위치로 갱신
		lastX = xpos;
		lastY = ypos;

		// DeltaX와 DeltaY 출력
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