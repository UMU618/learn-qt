// Mock the `vulkaninfo --summary` command

#include <cstdint>
#include <cstdlib>
#include <vector>

#include <QtCore/QLoggingCategory>
#include <QtGui/QGuiApplication>
#include <QtGui/QVulkanFunctions>
#include <QtGui/QVulkanInstance>

#include <vulkan/vulkan_core.h>

static Q_LOGGING_CATEGORY(lcVp, "VkInfo")

auto main(int argc, char* argv[]) -> int {
  // Should be QGuiApplication for Vulkan support
  // QCoreApplication a(argc, argv);
  const QGuiApplication app(argc, argv);
  qDebug(lcVp) << "Current Platform:" << QGuiApplication::platformName();

  QLoggingCategory::setFilterRules(QStringLiteral("qt.vulkan=true"));

  QVulkanInstance vulkan_instance;
#ifndef Q_OS_ANDROID
  vulkan_instance.setLayers(QByteArrayList()
                            << "VK_LAYER_LUNARG_standard_validation");
#else
  inst.setLayers(QByteArrayList() << "VK_LAYER_GOOGLE_threading"
                                  << "VK_LAYER_LUNARG_parameter_validation"
                                  << "VK_LAYER_LUNARG_object_tracker"
                                  << "VK_LAYER_LUNARG_core_validation"
                                  << "VK_LAYER_LUNARG_image"
                                  << "VK_LAYER_LUNARG_swapchain"
                                  << "VK_LAYER_GOOGLE_unique_objects");
#endif
  if (!vulkan_instance.create()) {
    qFatal("Failed to create Vulkan instance: %d", vulkan_instance.errorCode());
  }

  bool has_target_extension{false};
  const QByteArray kTargetExtension{
      VK_KHR_EXTERNAL_MEMORY_CAPABILITIES_EXTENSION_NAME};

  auto supported_extensions = vulkan_instance.supportedExtensions();
  qInfo(lcVp, "Instance Extensions Supported: count = %llu",
        supported_extensions.size());
  for (const auto& ext : supported_extensions) {
    qInfo(lcVp, "  %s, v%u", ext.name.constData(), ext.version);

    if (!has_target_extension) {
      if (0 == kTargetExtension.compare(ext.name)) {
        has_target_extension = true;
      }
    }
  }

  if (!has_target_extension) {
    qCritical(lcVp, "%s not found!", kTargetExtension.constData());
    return EXIT_FAILURE;
  }

  QVulkanFunctions* f = vulkan_instance.functions();
  uint32_t physical_device_count{};
  VkResult err = f->vkEnumeratePhysicalDevices(vulkan_instance.vkInstance(),
                                               &physical_device_count, nullptr);
  if (err != VK_SUCCESS) {
    qCritical(lcVp, "Failed to get physical device count: %d", err);
    return EXIT_FAILURE;
  }

  std::vector<VkPhysicalDevice> physical_devices(physical_device_count);
  err = f->vkEnumeratePhysicalDevices(vulkan_instance.vkInstance(),
                                      &physical_device_count,
                                      physical_devices.data());
  if (err != VK_SUCCESS) {
    qCritical(lcVp, "Failed to get %u physical devices: %d",
              physical_device_count, err);
    return EXIT_FAILURE;
  }
  qInfo(lcVp, "Physical Devices: count = %u", physical_device_count);

  for (uint32_t i = 0; i < physical_device_count; ++i) {
    const auto& physical_device = physical_devices.at(i);
    VkPhysicalDeviceProperties physical_device_properties{};
    f->vkGetPhysicalDeviceProperties(physical_device,
                                     &physical_device_properties);

    qInfo(lcVp, "  [%u] %s", i, physical_device_properties.deviceName);
    qInfo(lcVp, "    apiVersion     = %u.%u.%u",
          VK_VERSION_MAJOR(physical_device_properties.apiVersion),
          VK_VERSION_MINOR(physical_device_properties.apiVersion),
          VK_VERSION_PATCH(physical_device_properties.apiVersion));
    qInfo(lcVp, "    driverVersion  = %u.%u.%u",
          VK_VERSION_MAJOR(physical_device_properties.driverVersion),
          VK_VERSION_MINOR(physical_device_properties.driverVersion),
          VK_VERSION_PATCH(physical_device_properties.driverVersion));
    qInfo(lcVp, "    vendorID       = 0x%04x",
          physical_device_properties.vendorID);
    qInfo(lcVp, "    deviceID       = 0x%04x",
          physical_device_properties.deviceID);
    qInfo(lcVp, "    deviceType     = 0x%04x",
          physical_device_properties.deviceType);

    VkPhysicalDeviceMemoryProperties memory_properties{};
    f->vkGetPhysicalDeviceMemoryProperties(physical_device, &memory_properties);
    qInfo(lcVp, "    memoryTypeCount = %u", memory_properties.memoryTypeCount);
    for (uint32_t j = 0; j < memory_properties.memoryTypeCount; ++j) {
      qInfo(lcVp, "      [%u] heapIndex = %u, propertyFlags = %u", j,
            memory_properties.memoryTypes[j].heapIndex,
            memory_properties.memoryTypes[j].propertyFlags);
    }

    uint32_t extension_count{};
    err = f->vkEnumerateDeviceExtensionProperties(physical_device, nullptr,
                                                  &extension_count, nullptr);
    if (err != VK_SUCCESS) {
      qCritical(lcVp, "Failed to get physical device extensions: %d", err);
      continue;
    }

    std::vector<VkExtensionProperties> extensions(extension_count);
    err = f->vkEnumerateDeviceExtensionProperties(
        physical_device, nullptr, &extension_count, extensions.data());
    if (err != VK_SUCCESS) {
      qCritical(lcVp, "Failed to get %u physical device extensions: %d",
                extension_count, err);
      continue;
    }

    qInfo(lcVp, "    Device Extensions: count = %u", extension_count);
    for (uint32_t j = 0; j < extension_count; ++j) {
      qInfo(lcVp, "      [%u] %s, v%u", j, extensions.at(j).extensionName,
            extensions.at(j).specVersion);
    }
  }

  return EXIT_SUCCESS;
}
