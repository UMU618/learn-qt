#include "vk_triangle_window.h"

QVulkanWindowRenderer* VkTriangleWindow::createRenderer() noexcept {
  return new TriangleRenderer(this, true);  // try MSAA, when available
}
