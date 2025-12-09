#include "triangle_renderer.h"

#include <QFile>
#include <QVulkanFunctions>

namespace {
// Note that the vertex data and the projection matrix assume OpenGL. With
// Vulkan Y is negated in clip space and the near/far plane is at 0/1 instead
// of -1/1. These will be corrected for by an extra transformation when
// calculating the modelview-projection matrix.
float vertex_data[] = {
    // Y up, front = CCW
    // x    y     R     G     B
    0.0f,  0.5f,  1.0f, 0.0f, 0.0f,  // 0
    -0.5f, -0.5f, 0.0f, 1.0f, 0.0f,  // 1
    0.5f,  -0.5f, 0.0f, 0.0f, 1.0f   // 2
};

constexpr int kUniformDataSize{16 * sizeof(float)};

inline VkDeviceSize Aligned(VkDeviceSize v, VkDeviceSize byte_align) {
  return (v + byte_align - 1) & ~(byte_align - 1);
}
}  // namespace

TriangleRenderer::TriangleRenderer(QVulkanWindow* w, bool msaa) noexcept
    : window_(w) {
  w->setPreferredColorFormats(
      {VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM});
  if (msaa) {
    const QVector<int> counts = w->supportedSampleCounts();
    qDebug() << "Supported sample counts:" << counts;
    for (int s = 16; s >= 4; s /= 2) {
      if (counts.contains(s)) {
        qDebug("Requesting sample count %d", s);
        window_->setSampleCount(s);
        break;
      }
    }
  }
}

void TriangleRenderer::initResources() noexcept {
  qDebug("initResources");

  VkDevice device = window_->device();
  device_functions_ = window_->vulkanInstance()->deviceFunctions(device);

  // Prepare the vertex and uniform data. The vertex data will never
  // change so one buffer is sufficient regardless of the value of
  // QVulkanWindow::CONCURRENT_FRAME_COUNT. Uniform data is changing per
  // frame however so active frames have to have a dedicated copy.

  // Use just one memory allocation and one buffer. We will then specify the
  // appropriate offsets for uniform buffers in the VkDescriptorBufferInfo.
  // Have to watch out for
  // VkPhysicalDeviceLimits::minUniformBufferOffsetAlignment, though.

  // The uniform buffer is not strictly required in this example, we could
  // have used push constants as well since our single matrix (64 bytes) fits
  // into the spec mandated minimum limit of 128 bytes. However, once that
  // limit is not sufficient, the per-frame buffers, as shown below, will
  // become necessary.

  const int concurrent_frame_count = window_->concurrentFrameCount();
  const VkPhysicalDeviceLimits* device_limits =
      &window_->physicalDeviceProperties()->limits;
  const VkDeviceSize alignment = device_limits->minUniformBufferOffsetAlignment;
  qDebug("uniform buffer offset alignment is %u", (uint)alignment);

  // Our internal layout is vertex, uniform, uniform, ... with each uniform
  // buffer start offset aligned to alignment.
  const VkDeviceSize kVertexAllocSize = Aligned(sizeof(vertex_data), alignment);
  const VkDeviceSize kUniformAllocSize = Aligned(kUniformDataSize, alignment);
  VkBufferCreateInfo buffer_info{
      .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
      .size = kVertexAllocSize + concurrent_frame_count * kUniformAllocSize,
      .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
               VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT};

  VkResult err = device_functions_->vkCreateBuffer(device, &buffer_info,
                                                   nullptr, &buffer_);
  if (err != VK_SUCCESS) {
    qFatal("Failed to create buffer: %d", err);
  }

  VkMemoryRequirements mem_req;
  device_functions_->vkGetBufferMemoryRequirements(device, buffer_, &mem_req);

  VkMemoryAllocateInfo mem_alloc_info{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
                                      nullptr, mem_req.size,
                                      window_->hostVisibleMemoryIndex()};

  err = device_functions_->vkAllocateMemory(device, &mem_alloc_info, nullptr,
                                            &device_memory_);
  if (err != VK_SUCCESS) {
    qFatal("Failed to allocate memory: %d", err);
  }

  err =
      device_functions_->vkBindBufferMemory(device, buffer_, device_memory_, 0);
  if (err != VK_SUCCESS) {
    qFatal("Failed to bind buffer memory: %d", err);
  }

  quint8* p;
  err = device_functions_->vkMapMemory(device, device_memory_, 0, mem_req.size,
                                       0, reinterpret_cast<void**>(&p));
  if (err != VK_SUCCESS) {
    qFatal("Failed to map memory: %d", err);
  }
  memcpy(p, vertex_data, sizeof(vertex_data));
  QMatrix4x4 ident;
  memset(buffer_info_, 0, sizeof(buffer_info_));
  for (int i = 0; i < concurrent_frame_count; ++i) {
    const VkDeviceSize offset = kVertexAllocSize + i * kUniformAllocSize;
    memcpy(p + offset, ident.constData(), 16 * sizeof(float));
    buffer_info_[i].buffer = buffer_;
    buffer_info_[i].offset = offset;
    buffer_info_[i].range = kUniformAllocSize;
  }
  device_functions_->vkUnmapMemory(device, device_memory_);

  // Set up descriptor set and its layout.
  VkDescriptorPoolSize desc_pool_sizes = {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                          uint32_t(concurrent_frame_count)};
  VkDescriptorPoolCreateInfo desc_pool_info{
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
      .maxSets = uint32_t(concurrent_frame_count),
      .poolSizeCount = 1,
      .pPoolSizes = &desc_pool_sizes};
  err = device_functions_->vkCreateDescriptorPool(device, &desc_pool_info,
                                                  nullptr, &desc_pool_);
  if (err != VK_SUCCESS) {
    qFatal("Failed to create descriptor pool: %d", err);
  }

  VkDescriptorSetLayoutBinding layoutBinding = {
      0,  // binding
      VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT,
      nullptr};
  VkDescriptorSetLayoutCreateInfo descLayoutInfo = {
      VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, nullptr, 0, 1,
      &layoutBinding};
  err = device_functions_->vkCreateDescriptorSetLayout(
      device, &descLayoutInfo, nullptr, &desc_set_layout_);
  if (err != VK_SUCCESS) {
    qFatal("Failed to create descriptor set layout: %d", err);
  }

  for (int i = 0; i < concurrent_frame_count; ++i) {
    VkDescriptorSetAllocateInfo desc_set_alloc_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .pNext = nullptr,
        .descriptorPool = desc_pool_,
        .descriptorSetCount = 1,
        .pSetLayouts = &desc_set_layout_};
    err = device_functions_->vkAllocateDescriptorSets(
        device, &desc_set_alloc_info, &desc_set_[i]);
    if (err != VK_SUCCESS) {
      qFatal("Failed to allocate descriptor set: %d", err);
    }

    VkWriteDescriptorSet desc_write{
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = desc_set_[i],
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .pBufferInfo = &buffer_info_[i]};
    device_functions_->vkUpdateDescriptorSets(device, 1, &desc_write, 0,
                                              nullptr);
  }

  // Pipeline cache
  VkPipelineCacheCreateInfo pipeline_cache_info{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO};
  err = device_functions_->vkCreatePipelineCache(device, &pipeline_cache_info,
                                                 nullptr, &pipeline_cache_);
  if (err != VK_SUCCESS) {
    qFatal("Failed to create pipeline cache: %d", err);
  }

  // Pipeline layout
  VkPipelineLayoutCreateInfo pipeline_layout_info{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
      .setLayoutCount = 1,
      .pSetLayouts = &desc_set_layout_};
  err = device_functions_->vkCreatePipelineLayout(device, &pipeline_layout_info,
                                                  nullptr, &pipeline_layout_);
  if (err != VK_SUCCESS) {
    qFatal("Failed to create pipeline layout: %d", err);
  }

  VkPipelineInputAssemblyStateCreateInfo ia{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
      .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST};

  // The viewport and scissor will be set dynamically via
  // vkCmdSetViewport/Scissor. This way the pipeline does not need to be touched
  // when resizing the window.
  VkPipelineViewportStateCreateInfo vp{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
      .viewportCount = 1,
      .scissorCount = 1};

  VkPipelineRasterizationStateCreateInfo rs{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
      .polygonMode = VK_POLYGON_MODE_FILL,
      .cullMode = VK_CULL_MODE_NONE,  // we want the back face as well
      .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
      .lineWidth = 1.0f};

  VkPipelineMultisampleStateCreateInfo ms{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
      // Enable multisampling.
      .rasterizationSamples = window_->sampleCountFlagBits()};

  VkPipelineDepthStencilStateCreateInfo ds{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
      .depthTestEnable = VK_TRUE,
      .depthWriteEnable = VK_TRUE,
      .depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL};

  VkPipelineColorBlendAttachmentState att{.colorWriteMask = 0xF};
  VkPipelineColorBlendStateCreateInfo cb{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
      // no blend, write out all of rgba
      .attachmentCount = 1,
      .pAttachments = &att};

  VkDynamicState dyn_enable[] = {VK_DYNAMIC_STATE_VIEWPORT,
                                 VK_DYNAMIC_STATE_SCISSOR};
  VkPipelineDynamicStateCreateInfo dyn{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
      .dynamicStateCount = sizeof(dyn_enable) / sizeof(VkDynamicState),
      .pDynamicStates = dyn_enable};

  // Shaders
  VkShaderModule vert_shader_module =
      CreateShader(QStringLiteral(":/color_vert.spv"));
  VkShaderModule frag_shader_module =
      CreateShader(QStringLiteral(":/color_frag.spv"));

  VkPipelineShaderStageCreateInfo shader_stages[2] = {
      {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
       .pNext = nullptr,
       .flags = 0,
       .stage = VK_SHADER_STAGE_VERTEX_BIT,
       .module = vert_shader_module,
       .pName = "main",
       .pSpecializationInfo = nullptr},
      {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
       .pNext = nullptr,
       .flags = 0,
       .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
       .module = frag_shader_module,
       .pName = "main",
       .pSpecializationInfo = nullptr}};

  // Graphics pipeline
  VkVertexInputBindingDescription vertex_binding_desc = {
      .binding = 0,
      .stride = 5 * sizeof(float),
      .inputRate = VK_VERTEX_INPUT_RATE_VERTEX};
  VkVertexInputAttributeDescription vertex_attr_desc[] = {
      // position
      {.location = 0,
       .binding = 0,
       .format = VK_FORMAT_R32G32_SFLOAT,
       .offset = 0},
      // color
      {.location = 1,
       .binding = 0,
       .format = VK_FORMAT_R32G32B32_SFLOAT,
       .offset = 2 * sizeof(float)}};
  VkPipelineVertexInputStateCreateInfo vertex_input_info{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
      .vertexBindingDescriptionCount = 1,
      .pVertexBindingDescriptions = &vertex_binding_desc,
      .vertexAttributeDescriptionCount = 2,
      .pVertexAttributeDescriptions = vertex_attr_desc};
  VkGraphicsPipelineCreateInfo pipeline_info{
      .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
      .stageCount = 2,
      .pStages = shader_stages,
      .pVertexInputState = &vertex_input_info,
      .pInputAssemblyState = &ia,
      .pViewportState = &vp,
      .pRasterizationState = &rs,
      .pMultisampleState = &ms,
      .pDepthStencilState = &ds,
      .pColorBlendState = &cb,
      .pDynamicState = &dyn,
      .layout = pipeline_layout_,
      .renderPass = window_->defaultRenderPass()};

  err = device_functions_->vkCreateGraphicsPipelines(
      device, pipeline_cache_, 1, &pipeline_info, nullptr, &pipeline_);
  if (err != VK_SUCCESS) {
    qFatal("Failed to create graphics pipeline: %d", err);
  }

  if (vert_shader_module) {
    device_functions_->vkDestroyShaderModule(device, vert_shader_module,
                                             nullptr);
  }
  if (frag_shader_module) {
    device_functions_->vkDestroyShaderModule(device, frag_shader_module,
                                             nullptr);
  }
}

void TriangleRenderer::initSwapChainResources() noexcept {
  qDebug("initSwapChainResources");

  // Projection matrix
  projection_ = window_->clipCorrectionMatrix();  // adjust for Vulkan-OpenGL
                                                  // clip space differences
  const QSize sz = window_->swapChainImageSize();
  projection_.perspective(45.0f, sz.width() / (float)sz.height(), 0.01f,
                          100.0f);
  projection_.translate(0, 0, -4);
}

void TriangleRenderer::releaseSwapChainResources() noexcept {
  qDebug("releaseSwapChainResources");
}

void TriangleRenderer::releaseResources() noexcept {
  qDebug("releaseResources");

  VkDevice dev = window_->device();

  if (pipeline_) {
    device_functions_->vkDestroyPipeline(dev, pipeline_, nullptr);
    pipeline_ = VK_NULL_HANDLE;
  }

  if (pipeline_layout_) {
    device_functions_->vkDestroyPipelineLayout(dev, pipeline_layout_, nullptr);
    pipeline_layout_ = VK_NULL_HANDLE;
  }

  if (pipeline_cache_) {
    device_functions_->vkDestroyPipelineCache(dev, pipeline_cache_, nullptr);
    pipeline_cache_ = VK_NULL_HANDLE;
  }

  if (desc_set_layout_) {
    device_functions_->vkDestroyDescriptorSetLayout(dev, desc_set_layout_,
                                                    nullptr);
    desc_set_layout_ = VK_NULL_HANDLE;
  }

  if (desc_pool_) {
    device_functions_->vkDestroyDescriptorPool(dev, desc_pool_, nullptr);
    desc_pool_ = VK_NULL_HANDLE;
  }

  if (buffer_) {
    device_functions_->vkDestroyBuffer(dev, buffer_, nullptr);
    buffer_ = VK_NULL_HANDLE;
  }

  if (device_memory_) {
    device_functions_->vkFreeMemory(dev, device_memory_, nullptr);
    device_memory_ = VK_NULL_HANDLE;
  }
}

void TriangleRenderer::startNextFrame() noexcept {
  VkClearColorValue clear_color{.float32{0.0f, 0.25f, 0.0f, 1.0f}};
  VkClearDepthStencilValue clear_ds{.depth = 1.0f, .stencil = 0};
  VkClearValue clear_values[3]{{.color = clear_color},
                               {.depthStencil = clear_ds},
                               {.color = clear_color}};

  VkCommandBuffer cb = window_->currentCommandBuffer();
  const QSize sz = window_->swapChainImageSize();
  VkRenderPassBeginInfo render_pass_begin_info{
      .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
      .renderPass = window_->defaultRenderPass(),
      .framebuffer = window_->currentFramebuffer(),
      .renderArea = {.extent = {.width = uint32_t(sz.width()),
                                .height = uint32_t(sz.height())}},
      .clearValueCount =
          window_->sampleCountFlagBits() > VK_SAMPLE_COUNT_1_BIT ? 3U : 2U,
      .pClearValues = clear_values};
  device_functions_->vkCmdBeginRenderPass(cb, &render_pass_begin_info,
                                          VK_SUBPASS_CONTENTS_INLINE);

  VkDevice device = window_->device();
  quint8* p;
  VkResult err = device_functions_->vkMapMemory(
      device, device_memory_, buffer_info_[window_->currentFrame()].offset,
      kUniformDataSize, 0, reinterpret_cast<void**>(&p));
  if (err != VK_SUCCESS) {
    qFatal("Failed to map memory: %d", err);
  }
  QMatrix4x4 m = projection_;
  m.rotate(rotation_, 0, 1, 0);
  memcpy(p, m.constData(), 16 * sizeof(float));
  device_functions_->vkUnmapMemory(device, device_memory_);

  // Not exactly a real animation system, just advance on every frame for now.
  rotation_ += 1.0f;

  device_functions_->vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                       pipeline_);
  device_functions_->vkCmdBindDescriptorSets(
      cb, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout_, 0, 1,
      &desc_set_[window_->currentFrame()], 0, nullptr);
  VkDeviceSize vertex_buffers_offset = 0;
  device_functions_->vkCmdBindVertexBuffers(cb, 0, 1, &buffer_,
                                            &vertex_buffers_offset);

  VkViewport viewport{.x = 0,
                      .y = 0,
                      .width = float(sz.width()),
                      .height = float(sz.height()),
                      .minDepth = 0,
                      .maxDepth = 1};
  device_functions_->vkCmdSetViewport(cb, 0, 1, &viewport);

  VkRect2D scissor{.offset{.x = 0, .y = 0},
                   .extent = render_pass_begin_info.renderArea.extent};
  device_functions_->vkCmdSetScissor(cb, 0, 1, &scissor);

  device_functions_->vkCmdDraw(cb, 3, 1, 0, 0);

  device_functions_->vkCmdEndRenderPass(cb);

  window_->frameReady();
  window_->requestUpdate();  // render continuously, throttled by the
                             // presentation rate
}

VkShaderModule TriangleRenderer::CreateShader(const QString& name) {
  QFile file(name);
  if (!file.open(QIODevice::ReadOnly)) {
    qWarning("Failed to read shader %s", qPrintable(name));
    return VK_NULL_HANDLE;
  }
  QByteArray blob = file.readAll();
  file.close();

  VkShaderModuleCreateInfo shader_info{
      .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
      .codeSize = static_cast<size_t>(blob.size()),
      .pCode = reinterpret_cast<const uint32_t*>(blob.constData())};
  VkShaderModule shader_module;
  VkResult err = device_functions_->vkCreateShaderModule(
      window_->device(), &shader_info, nullptr, &shader_module);
  if (err != VK_SUCCESS) {
    qWarning("Failed to create shader module: %d", err);
    return VK_NULL_HANDLE;
  }

  return shader_module;
}
