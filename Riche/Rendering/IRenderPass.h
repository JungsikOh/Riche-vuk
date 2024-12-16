#pragma once

class Camera;

class IRenderPass {
 public:
  virtual void Initialize(VkDevice device, VkPhysicalDevice physicalDevice, VkQueue queue, VkCommandPool commandPool, Camera* camera,
                          const uint32_t width, const uint32_t height) = 0;
  virtual void Cleanup() = 0;

  virtual void Update() = 0;

  virtual void Draw(VkSemaphore renderAvailable) = 0;

 private:
  // - Rendering Pipeline
  virtual void CreateFramebuffer() = 0;
  virtual void CreateRenderPass() = 0;

  virtual void CreatePipeline() = 0;
  virtual void CreateBuffers() = 0;

  virtual void CreateCommandBuffers() = 0;
  virtual void RecordCommands() = 0;
};