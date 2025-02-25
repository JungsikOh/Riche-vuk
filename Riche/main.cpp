#include <cstdlib>  // std::system
#include <filesystem>
#include <string_view>

#include "Rendering/Camera.h"
#include "Rendering/VulkanRenderer.h"

GLFWwindow* window;
VulkanRenderer vulkanRenderer;
Camera g_camera;

static double deltaTime = 0.0;
static float smoothedDeltaTime = 0.016f;

double lastX = 0.0, lastY = 0.0;  // ���� �������� ���콺 ��ǥ
bool firstMouse = true;           // �ʱ� ���콺 ��ġ Ȯ���� ���� �÷���
bool rightButtonPressed = false;
bool leftButtonPressed = false;
bool hasLeftButtonPressed = false;

void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) { /*g_camera.OnKeyInput(deltaTime, key);*/ }

void MouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
  if (button == GLFW_MOUSE_BUTTON_RIGHT) {  // ���콺 ������ ��ư�� ���
    if (action == GLFW_PRESS) {             // ���� ��
      rightButtonPressed = true;
      firstMouse = true;                  // ���콺�� ���� ������ �ʱ�ȭ
    } else if (action == GLFW_RELEASE) {  // �� ��
      rightButtonPressed = false;
    }
  }

  if (ImGui::GetIO().WantCaptureMouse) {
    return;  // early out
  }

  if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
    leftButtonPressed = true;
    hasLeftButtonPressed = false;
    g_camera.isMousePressed = true;

    double xpos, ypos;
    glfwGetCursorPos(window, &xpos, &ypos);
    g_camera.SetMousePosition(glm::vec2(float(xpos), float(ypos)));

  } else if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_RELEASE) {
    leftButtonPressed = false;
    hasLeftButtonPressed = false;  // ���� �巡�׸� ���� Ǯ����
    g_camera.isMousePressed = false;
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
    double deltaX = (xpos - lastX);
    double deltaY = (ypos - lastY);

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

  //glfwSetKeyCallback(window, KeyCallback);
  glfwSetMouseButtonCallback(window, MouseButtonCallback);
  glfwSetCursorPosCallback(window, CursorPositionCallback);

  CameraParameters cameraParams = {};
  cameraParams.speed = 5.0f;
  cameraParams.sensitivity = 0.2f;
  cameraParams.position = glm::vec3(0.0f, 0.0f, 2.0f);
  cameraParams.lootAt = glm::vec3(0.0f, 0.0f, -1.0f);
  cameraParams.fov = 45.0f;
  cameraParams.aspectRatio = 1920.0f / 1080.0f;
  cameraParams.nearPlane = 0.05f;
  cameraParams.farPlane = 500.0f;
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
    float now = glfwGetTime();
    deltaTime = now - lastTime;
    lastTime = now;

    const float alpha = 0.1f;
    smoothedDeltaTime = (1.0f - alpha) * smoothedDeltaTime + alpha * deltaTime;

    deltaTime = smoothedDeltaTime;

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
      g_camera.MoveFoward(deltaTime);
    }
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
      g_camera.MoveFoward(-deltaTime);
    }
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
      g_camera.MoveRight(-deltaTime);
    }
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
      g_camera.MoveRight(deltaTime);
    }
    if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) {
      g_camera.MoveUp(deltaTime);
    }
    if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) {
      g_camera.MoveUp(-deltaTime);
    }

    glfwPollEvents();

    vulkanRenderer.Draw();
  }

  vulkanRenderer.Cleanup();

  // Destroy GLFW window and stop GLFW
  glfwDestroyWindow(window);
  glfwTerminate();

  return 0;
}