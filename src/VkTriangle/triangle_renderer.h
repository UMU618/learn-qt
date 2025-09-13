#pragma once

#include <QtGui/QVulkanWindow>

class TriangleRenderer : public QVulkanWindowRenderer {
 public:
  TriangleRenderer(QVulkanWindow* w, bool msaa = false) noexcept;

  void initResources() noexcept override;
  void initSwapChainResources() noexcept override;
  void releaseSwapChainResources() noexcept override;
  void releaseResources() noexcept override;

  void startNextFrame() noexcept override;

 private:
  VkShaderModule CreateShader(const QString& name);

 private:
  QVulkanWindow* window_;
  QVulkanDeviceFunctions* device_functions_;

  VkDeviceMemory device_memory_{VK_NULL_HANDLE};
  VkBuffer buffer_{VK_NULL_HANDLE};
  VkDescriptorBufferInfo
      buffer_info_[QVulkanWindow::MAX_CONCURRENT_FRAME_COUNT];

  VkDescriptorPool desc_pool_{VK_NULL_HANDLE};
  VkDescriptorSetLayout desc_set_layout_{VK_NULL_HANDLE};
  VkDescriptorSet desc_set_[QVulkanWindow::MAX_CONCURRENT_FRAME_COUNT];

  VkPipelineCache pipeline_cache_{VK_NULL_HANDLE};
  VkPipelineLayout pipeline_layout_{VK_NULL_HANDLE};
  VkPipeline pipeline_{VK_NULL_HANDLE};

  QMatrix4x4 projection_;
  float rotation_{0.0f};
};
