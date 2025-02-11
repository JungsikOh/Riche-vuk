#pragma once

static stbi_uc* LoadTextureFile(std::string filename, int* width, int* height, VkDeviceSize* imageSize) {
  // number of channels image uses
  int channels;

  // Load Pixel data for image
  std::string fileLoc = filename;
  stbi_uc* image = stbi_load(fileLoc.c_str(), width, height, &channels, STBI_rgb_alpha);

  if (!image) {
    throw std::runtime_error("Failed to Load a Texture file! (" + filename + ")");
  }

  // Calculate Image size using given and known data
  *imageSize = *width * *height * 4;

  return image;
}

inline VkTransformMatrixKHR mat4ToVkTransform(const glm::mat4& m) {
  VkTransformMatrixKHR t{};

  // Row 0 (x축)
  t.matrix[0][0] = m[0][0];  // col=0, row=0
  t.matrix[0][1] = m[1][0];  // col=1, row=0
  t.matrix[0][2] = m[2][0];  // col=2, row=0
  t.matrix[0][3] = m[3][0];  // col=3, row=0

  // Row 1 (y축)
  t.matrix[1][0] = m[0][1];
  t.matrix[1][1] = m[1][1];
  t.matrix[1][2] = m[2][1];
  t.matrix[1][3] = m[3][1];

  // Row 2 (z축)
  t.matrix[2][0] = m[0][2];
  t.matrix[2][1] = m[1][2];
  t.matrix[2][2] = m[2][2];
  t.matrix[2][3] = m[3][2];

  // 4번째 행( [0,0,0,1] )은 VkTransformMatrixKHR에 없음(암묵적)

  return t;
}