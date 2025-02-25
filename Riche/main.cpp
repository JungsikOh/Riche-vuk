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

double lastX = 0.0, lastY = 0.0;  // 이전 프레임의 마우스 좌표
bool firstMouse = true;           // 초기 마우스 위치 확인을 위한 플래그
bool rightButtonPressed = false;
bool leftButtonPressed = false;
bool hasLeftButtonPressed = false;

void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) { /*g_camera.OnKeyInput(deltaTime, key);*/ }

void MouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
  if (button == GLFW_MOUSE_BUTTON_RIGHT) {  // 마우스 오른쪽 버튼일 경우
    if (action == GLFW_PRESS) {             // 누를 때
      rightButtonPressed = true;
      firstMouse = true;                  // 마우스를 누를 때마다 초기화
    } else if (action == GLFW_RELEASE) {  // 뗄 때
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
    hasLeftButtonPressed = false;  // 다음 드래그를 위해 풀어줌
    g_camera.isMousePressed = false;
  }
}

void CursorPositionCallback(GLFWwindow* window, double xpos, double ypos) {
  if (rightButtonPressed) {
    if (firstMouse) {
      // 처음엔 이전 위치가 없기 때문에 초기 위치 설정
      lastX = xpos;
      lastY = ypos;
      firstMouse = false;
    }

    // DeltaX, DeltaY 계산
    double deltaX = (xpos - lastX);
    double deltaY = (ypos - lastY);

    // 현재 위치를 이전 위치로 갱신
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