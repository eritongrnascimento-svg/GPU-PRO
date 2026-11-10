#pragma once
#include "model.h"
#include <QVulkanInstance>
#include <vulkan/vulkan.h>
QJsonObject vulkanSpecifications(const Gpu &);
int vulkanDeviceIndex(QVulkanInstance &, const Gpu &);
