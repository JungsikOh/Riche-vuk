#include <filesystem>
#include <cstdlib>     // std::system
#include <string_view>

#include "Rendering/Camera.h"
#include "Rendering/VulkanRenderer.h"

GLFWwindow* window;
VulkanRenderer vulkanRenderer;
Camera g_camera;

float deltaTime = 0.0f;

double lastX = 0.0, lastY = 0.0;  // ���� �������� ���콺 ��ǥ
bool firstMouse = true;           // �ʱ� ���콺 ��ġ Ȯ���� ���� �÷���
bool rightButtonPressed = false;

void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) { g_camera.OnKeyInput(deltaTime, key); }

void MouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
  if (button == GLFW_MOUSE_BUTTON_RIGHT) {  // ���콺 ������ ��ư�� ���
    if (action == GLFW_PRESS) {             // ���� ��
      rightButtonPressed = true;
      firstMouse = true;  // ���콺�� ���� ������ �ʱ�ȭ
      std::cout << "Right Mouse Button Pressed" << std::endl;
    } else if (action == GLFW_RELEASE) {  // �� ��
      rightButtonPressed = false;
      std::cout << "Right Mouse Button Released" << std::endl;
    }
  }
}

void CursorPositionCallback(GLFWwindow* window, double xpos, double ypos) {
  if (rightButtonPressed) {
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

    g_camera.OnMouseInput(deltaX, deltaY);
  }
}

void InitWindow(std::string wName = "Test Window", const int w = 800, const int h = 600) {
  // Initialize GLFW
  glfwInit();

  // Set GLFW to Not work with OpenGL, For Vulkan
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

  window = glfwCreateWindow(w, h, wName.c_str(), nullptr, nullptr);
}

int main() {

  // Create Window
  InitWindow("Test Widnow", 1920, 1080);

  glfwSetKeyCallback(window, KeyCallback);
  glfwSetMouseButtonCallback(window, MouseButtonCallback);
  glfwSetCursorPosCallback(window, CursorPositionCallback);

  CameraParameters cameraParams = {};
  cameraParams.speed = 3000.0f;
  cameraParams.sensitivity = 0.2f;
  cameraParams.position = glm::vec3(0.0f, 0.0f, 2.0f);
  cameraParams.lootAt = glm::vec3(0.0f, 0.0f, -1.0f);
  cameraParams.fov = 45.0f;
  cameraParams.aspectRatio = 1920.0f / 1080.0f;
  cameraParams.nearPlane = 0.5f;
  cameraParams.farPlane = 5000.0f;
  g_camera.Initialize(cameraParams);

  // Create Vulkan Renderer Instance
  vulkanRenderer.Initialize(window, &g_camera);
  /*if (vulkanRenderer.Initialize(window, &g_camera) == EXIT_FAILURE)
  {
          return EXIT_FAILURE;
  }*/

  float angle = 0.0f;
  float lastTime = 0.0f;

  // Loop until closed
  while (!glfwWindowShouldClose(window)) {
    glfwPollEvents();

    float now = glfwGetTime();
    deltaTime = now - lastTime;
    lastTime = now;

    angle += 10.0f * deltaTime;
    if (angle > 360.0f) {
      angle -= 360.0f;
    }

    glm::mat4 firstModel(1.0f);
    glm::mat4 secondModel(1.0f);

    firstModel = glm::translate(firstModel, glm::vec3(-2.0f, 0.0f, -1.0f));
    firstModel = glm::rotate(firstModel, glm::radians(angle), glm::vec3(0.0f, 0.0f, 1.0f));

    secondModel = glm::translate(secondModel, glm::vec3(-1.0f, 0.0f, -1.0f));
    secondModel = glm::rotate(secondModel, glm::radians(angle * 50), glm::vec3(0.0f, 0.0f, 1.0f));

    vulkanRenderer.Update();

    vulkanRenderer.Draw();
  }

  vulkanRenderer.Cleanup();

  // Destroy GLFW window and stop GLFW
  glfwDestroyWindow(window);
  glfwTerminate();

  return 0;
}