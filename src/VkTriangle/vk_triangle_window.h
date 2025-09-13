#pragma once

#include "triangle_renderer.h"

class VkTriangleWindow : public QVulkanWindow {
 public:
  QVulkanWindowRenderer* createRenderer() noexcept override;
};
