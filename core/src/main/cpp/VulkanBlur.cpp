#include "VulkanBlur.h"

#include "KawaseBlur.h"
#include "VulkanBuffer.h"
#include "VulkanImage.h"

#include <android/log.h>
#include <algorithm>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>

#define LOG_TAG "QmBlurVk"
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace {

constexpr uint32_t kApi13 = VK_API_VERSION_1_3;

int deviceTypeRank(VkPhysicalDeviceType t) {
    switch (t) {
        case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU: return 4;
        case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: return 3;
        case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU: return 2;
        case VK_PHYSICAL_DEVICE_TYPE_CPU: return 1;
        default: return 0;
    }
}

bool hasExt(const std::vector<VkExtensionProperties>& exts, const char* name) {
    return std::any_of(exts.begin(), exts.end(),
                       [&](const VkExtensionProperties& p) { return std::strcmp(p.extensionName, name) == 0; });
}

void copyRows(uint8_t* dst, int dstStride, const uint8_t* src, int srcStride, int w, int h) {
    const int row = w * 4;
    if (dstStride == row && srcStride == row) {
        std::memcpy(dst, src, static_cast<size_t>(row) * static_cast<size_t>(h));
        return;
    }
    for (int y = 0; y < h; ++y) {
        std::memcpy(dst + static_cast<size_t>(y) * dstStride, src + static_cast<size_t>(y) * srcStride,
                    static_cast<size_t>(row));
    }
}

struct Engine {
    VkInstance instance = VK_NULL_HANDLE;
    VkPhysicalDevice physical = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkQueue queue = VK_NULL_HANDLE;
    VkCommandPool commandPool = VK_NULL_HANDLE;
    VkCommandBuffer cmd = VK_NULL_HANDLE;
    VkFence fence = VK_NULL_HANDLE;
    PFN_vkCmdPipelineBarrier2 cmdBarrier2 = nullptr;
    uint32_t queueFamily = 0;
    VulkanImage input;
    VulkanImage blitDst;
    VulkanBuffer staging;
    KawaseBlur blur;
    VkImageLayout inputLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    float radius = 0.0f;
    bool failed = false;
};

Engine gEng;
std::mutex gMu;

#define VK_TRY(expr)                                 \
    do {                                             \
        VkResult _r = (expr);                        \
        if (_r != VK_SUCCESS) {                      \
            LOGE("%s failed (%d)", #expr, (int)_r);  \
            return false;                            \
        }                                            \
    } while (0)

bool createInstance() {
    uint32_t api = VK_API_VERSION_1_0;
    auto enumerate = reinterpret_cast<PFN_vkEnumerateInstanceVersion>(
            vkGetInstanceProcAddr(VK_NULL_HANDLE, "vkEnumerateInstanceVersion"));
    if (!enumerate || enumerate(&api) != VK_SUCCESS || api < VK_API_VERSION_1_1) {
        LOGE("Vulkan 1.1 loader required");
        return false;
    }
    if (api > kApi13) api = kApi13;

    VkApplicationInfo app{};
    app.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app.pApplicationName = "QmBlur";
    app.apiVersion = api;
    VkInstanceCreateInfo ci{};
    ci.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    ci.pApplicationInfo = &app;
    VK_TRY(vkCreateInstance(&ci, nullptr, &gEng.instance));
    return true;
}

bool pickDevice() {
    uint32_t count = 0;
    VK_TRY(vkEnumeratePhysicalDevices(gEng.instance, &count, nullptr));
    if (count == 0) {
        LOGE("no VkPhysicalDevice");
        return false;
    }
    std::vector<VkPhysicalDevice> devices(count);
    VK_TRY(vkEnumeratePhysicalDevices(gEng.instance, &count, devices.data()));

    VkPhysicalDevice best = VK_NULL_HANDLE;
    uint32_t bestFamily = 0;
    int bestRank = -1;
    uint32_t bestApi = 0;
    for (VkPhysicalDevice pd : devices) {
        VkPhysicalDeviceProperties props{};
        vkGetPhysicalDeviceProperties(pd, &props);
        if (props.apiVersion < VK_API_VERSION_1_1) continue;
        uint32_t qn = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(pd, &qn, nullptr);
        std::vector<VkQueueFamilyProperties> qprops(qn);
        vkGetPhysicalDeviceQueueFamilyProperties(pd, &qn, qprops.data());
        for (uint32_t i = 0; i < qn; ++i) {
            if ((qprops[i].queueFlags & VK_QUEUE_COMPUTE_BIT) == 0) continue;
            const int rank = deviceTypeRank(props.deviceType);
            if (rank > bestRank || (rank == bestRank && props.apiVersion > bestApi)) {
                best = pd;
                bestFamily = i;
                bestRank = rank;
                bestApi = props.apiVersion;
            }
            break;
        }
    }
    if (best == VK_NULL_HANDLE) {
        LOGE("no GPU with COMPUTE + Vulkan 1.1");
        return false;
    }
    gEng.physical = best;
    gEng.queueFamily = bestFamily;
    return true;
}

bool createDevice() {
    VkPhysicalDeviceProperties props{};
    vkGetPhysicalDeviceProperties(gEng.physical, &props);

    uint32_t extCount = 0;
    vkEnumerateDeviceExtensionProperties(gEng.physical, nullptr, &extCount, nullptr);
    std::vector<VkExtensionProperties> exts(extCount);
    if (extCount) vkEnumerateDeviceExtensionProperties(gEng.physical, nullptr, &extCount, exts.data());

    const bool vulkan13 = props.apiVersion >= VK_API_VERSION_1_3;
    const bool sync2Ext = hasExt(exts, VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME);
    std::vector<const char*> enabled;

    VkPhysicalDeviceSynchronization2Features sync2Available{};
    sync2Available.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES;
    auto getFeatures2 = reinterpret_cast<PFN_vkGetPhysicalDeviceFeatures2>(
            vkGetInstanceProcAddr(gEng.instance, "vkGetPhysicalDeviceFeatures2"));
    if (getFeatures2) {
        VkPhysicalDeviceFeatures2 features2{};
        features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
        features2.pNext = &sync2Available;
        getFeatures2(gEng.physical, &features2);
    }

    VkPhysicalDeviceSynchronization2Features sync2{};
    sync2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES;
    sync2.synchronization2 = VK_TRUE;
    void* pNext = nullptr;
    if (sync2Available.synchronization2 && (vulkan13 || sync2Ext)) {
        if (!vulkan13) enabled.push_back(VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME);
        pNext = &sync2;
    }

    float prio = 1.0f;
    VkDeviceQueueCreateInfo qci{};
    qci.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    qci.queueFamilyIndex = gEng.queueFamily;
    qci.queueCount = 1;
    qci.pQueuePriorities = &prio;
    VkDeviceCreateInfo ci{};
    ci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    ci.pNext = pNext;
    ci.queueCreateInfoCount = 1;
    ci.pQueueCreateInfos = &qci;
    ci.enabledExtensionCount = static_cast<uint32_t>(enabled.size());
    ci.ppEnabledExtensionNames = enabled.empty() ? nullptr : enabled.data();
    VK_TRY(vkCreateDevice(gEng.physical, &ci, nullptr, &gEng.device));
    vkGetDeviceQueue(gEng.device, gEng.queueFamily, 0, &gEng.queue);
    if (pNext) {
        gEng.cmdBarrier2 = reinterpret_cast<PFN_vkCmdPipelineBarrier2>(
                vkGetDeviceProcAddr(gEng.device, "vkCmdPipelineBarrier2"));
        if (!gEng.cmdBarrier2) {
            gEng.cmdBarrier2 = reinterpret_cast<PFN_vkCmdPipelineBarrier2>(
                    vkGetDeviceProcAddr(gEng.device, "vkCmdPipelineBarrier2KHR"));
        }
    }
    return true;
}

bool createCommands() {
    VkCommandPoolCreateInfo pci{};
    pci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    pci.queueFamilyIndex = gEng.queueFamily;
    VK_TRY(vkCreateCommandPool(gEng.device, &pci, nullptr, &gEng.commandPool));
    VkCommandBufferAllocateInfo ai{};
    ai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    ai.commandPool = gEng.commandPool;
    ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    ai.commandBufferCount = 1;
    VK_TRY(vkAllocateCommandBuffers(gEng.device, &ai, &gEng.cmd));
    VkFenceCreateInfo fi{};
    fi.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fi.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    VK_TRY(vkCreateFence(gEng.device, &fi, nullptr, &gEng.fence));
    return true;
}

bool initLocked() {
    if (gEng.device != VK_NULL_HANDLE) return true;
    if (gEng.failed) return false;
    if (!createInstance() || !pickDevice() || !createDevice() || !createCommands()) {
        gEng.failed = true;
        return false;
    }
    return true;
}

bool ensureStaging(VkDeviceSize bytes, std::string* error) {
    if (gEng.staging.size >= bytes) return true;
    return gEng.staging.create(
            gEng.device, gEng.physical, bytes,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, error);
}

bool blurLocked(uint8_t* rgba, int w, int h, int stride, float radius) {
    std::string error;
    VK_TRY(vkWaitForFences(gEng.device, 1, &gEng.fence, VK_TRUE, UINT64_MAX));

    if (gEng.input.width != static_cast<uint32_t>(w) || gEng.input.height != static_cast<uint32_t>(h)) {
        if (!gEng.input.create(gEng.device, gEng.physical, static_cast<uint32_t>(w), static_cast<uint32_t>(h),
                               VK_FORMAT_R8G8B8A8_UNORM,
                               VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, &error)) {
            LOGE("%s", error.c_str());
            return false;
        }
        gEng.inputLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    }

    const VkDeviceSize bytes = static_cast<VkDeviceSize>(w) * static_cast<VkDeviceSize>(h) * 4;
    if (!ensureStaging(bytes, &error)) {
        LOGE("%s", error.c_str());
        return false;
    }
    auto* mapped = static_cast<uint8_t*>(gEng.staging.map(&error));
    if (!mapped) {
        LOGE("%s", error.c_str());
        return false;
    }
    copyRows(mapped, w * 4, rgba, stride, w, h);
    gEng.staging.unmap();

    if (gEng.blur.passes() == 0) {
        if (!gEng.blur.create(gEng.device, gEng.physical, gEng.input, radius, gEng.queueFamily, gEng.cmdBarrier2,
                              &error)) {
            LOGE("%s", error.c_str());
            return false;
        }
        gEng.radius = radius;
    } else if (gEng.radius != radius) {
        if (!gEng.blur.setRadius(radius, gEng.input, &error)) {
            LOGE("%s", error.c_str());
            return false;
        }
        gEng.radius = radius;
    } else if (!gEng.blur.resize(gEng.input, &error)) {
        LOGE("%s", error.c_str());
        return false;
    }

    VK_TRY(vkResetCommandBuffer(gEng.cmd, 0));
    VkCommandBufferBeginInfo begin{};
    begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    VK_TRY(vkBeginCommandBuffer(gEng.cmd, &begin));

    imageBarrier(gEng.cmd, gEng.cmdBarrier2, gEng.input.image, VK_PIPELINE_STAGE_2_NONE, 0, gEng.inputLayout,
                 VK_PIPELINE_STAGE_2_COPY_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    VkBufferImageCopy region{};
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.layerCount = 1;
    region.imageExtent = {gEng.input.width, gEng.input.height, 1};
    vkCmdCopyBufferToImage(gEng.cmd, gEng.staging.buffer, gEng.input.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
                           &region);
    imageBarrier(gEng.cmd, gEng.cmdBarrier2, gEng.input.image, VK_PIPELINE_STAGE_2_COPY_BIT,
                 VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                 VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                 VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    gEng.inputLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    if (!gEng.blur.record(gEng.cmd, &error)) {
        LOGE("%s", error.empty() ? "KawaseBlur::record failed" : error.c_str());
        return false;
    }

    const VulkanImage* src = &gEng.blur.presentImage();
    if (src->width != static_cast<uint32_t>(w) || src->height != static_cast<uint32_t>(h)) {
        // vulkanfx blits the pyramid (often 1/4) to the surface; do that onto a full-size image.
        if (gEng.blitDst.width != static_cast<uint32_t>(w) || gEng.blitDst.height != static_cast<uint32_t>(h)) {
            if (!gEng.blitDst.create(gEng.device, gEng.physical, static_cast<uint32_t>(w), static_cast<uint32_t>(h),
                                     VK_FORMAT_R8G8B8A8_UNORM,
                                     VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, &error)) {
                LOGE("%s", error.c_str());
                return false;
            }
        }
        imageBarrier(gEng.cmd, gEng.cmdBarrier2, gEng.blitDst.image, VK_PIPELINE_STAGE_2_NONE, 0,
                     VK_IMAGE_LAYOUT_UNDEFINED, VK_PIPELINE_STAGE_2_BLIT_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT,
                     VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        VkImageBlit blit{};
        blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.srcSubresource.layerCount = 1;
        blit.srcOffsets[1] = {static_cast<int32_t>(src->width), static_cast<int32_t>(src->height), 1};
        blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.dstSubresource.layerCount = 1;
        blit.dstOffsets[1] = {w, h, 1};
        vkCmdBlitImage(gEng.cmd, src->image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, gEng.blitDst.image,
                       VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_LINEAR);
        imageBarrier(gEng.cmd, gEng.cmdBarrier2, gEng.blitDst.image, VK_PIPELINE_STAGE_2_BLIT_BIT,
                     VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                     VK_PIPELINE_STAGE_2_COPY_BIT, VK_ACCESS_2_TRANSFER_READ_BIT, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
        src = &gEng.blitDst;
    }

    VkBufferImageCopy outRegion{};
    outRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    outRegion.imageSubresource.layerCount = 1;
    outRegion.imageExtent = {src->width, src->height, 1};
    vkCmdCopyImageToBuffer(gEng.cmd, src->image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, gEng.staging.buffer, 1,
                           &outRegion);
    VkBufferMemoryBarrier hostBarrier{};
    hostBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    hostBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    hostBarrier.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
    hostBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    hostBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    hostBarrier.buffer = gEng.staging.buffer;
    hostBarrier.size = VK_WHOLE_SIZE;
    vkCmdPipelineBarrier(gEng.cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 0, nullptr, 1,
                         &hostBarrier, 0, nullptr);

    VK_TRY(vkEndCommandBuffer(gEng.cmd));
    VK_TRY(vkResetFences(gEng.device, 1, &gEng.fence));
    VkSubmitInfo submit{};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &gEng.cmd;
    VK_TRY(vkQueueSubmit(gEng.queue, 1, &submit, gEng.fence));
    VK_TRY(vkWaitForFences(gEng.device, 1, &gEng.fence, VK_TRUE, UINT64_MAX));

    mapped = static_cast<uint8_t*>(gEng.staging.map(&error));
    if (!mapped) {
        LOGE("%s", error.c_str());
        return false;
    }
    copyRows(rgba, stride, mapped, w * 4, w, h);
    gEng.staging.unmap();
    return true;
}

}  // namespace

int vulkan_kawase_blur_rgba(unsigned char* rgba, int w, int h, int stride, float radius, int rounds) {
    if (!rgba || w <= 0 || h <= 0 || stride < w * 4) return 0;
    if (radius < 1.0f) radius = 1.0f;
    if (rounds < 1) rounds = 1;
    radius *= static_cast<float>(rounds);
    std::lock_guard<std::mutex> lock(gMu);
    if (!initLocked()) return 0;
    return blurLocked(rgba, w, h, stride, radius) ? 1 : 0;
}
