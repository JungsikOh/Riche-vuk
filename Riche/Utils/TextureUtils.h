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