#include "VulkanDualKawase.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <dlfcn.h>

#include <android/log.h>
#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>

#include "vulkan/dual_kawase_down4_spv.h"
#include "vulkan/dual_kawase_down5_spv.h"
#include "vulkan/dual_kawase_up8_spv.h"

#define VKLOG_TAG "QmBlurVk"
#define VKLOGE(...) __android_log_print(ANDROID_LOG_ERROR, VKLOG_TAG, __VA_ARGS__)

typedef struct {
    int srcWidth;
    int srcHeight;
    int dstWidth;
    int dstHeight;
    int step;
} Push;

typedef struct {
    VkBuffer buffer;
    VkDeviceMemory memory;
    void* mapped;
    VkDeviceSize size;
} HostBuffer;

typedef struct {
    int initialized;
    int failed;
    void* vulkanLib;
    VkInstance instance;
    VkPhysicalDevice physicalDevice;
    VkDevice device;
    uint32_t queueFamilyIndex;
    VkQueue queue;
    VkCommandPool commandPool;
    VkDescriptorSetLayout descriptorSetLayout;
    VkPipelineLayout pipelineLayout;
    VkDescriptorPool descriptorPool;
    VkPipeline pipelineDown4;
    VkPipeline pipelineDown5;
    VkPipeline pipelineUp8;
} VulkanCtx;

static VulkanCtx gCtx;
static pthread_mutex_t gVkMutex = PTHREAD_MUTEX_INITIALIZER;

static PFN_vkGetInstanceProcAddr pfn_vkGetInstanceProcAddr;
static PFN_vkGetDeviceProcAddr pfn_vkGetDeviceProcAddr;
static PFN_vkCreateInstance pfn_vkCreateInstance;
static PFN_vkDestroyInstance pfn_vkDestroyInstance;
static PFN_vkEnumeratePhysicalDevices pfn_vkEnumeratePhysicalDevices;
static PFN_vkGetPhysicalDeviceQueueFamilyProperties pfn_vkGetPhysicalDeviceQueueFamilyProperties;
static PFN_vkGetPhysicalDeviceMemoryProperties pfn_vkGetPhysicalDeviceMemoryProperties;
static PFN_vkCreateDevice pfn_vkCreateDevice;
static PFN_vkDestroyDevice pfn_vkDestroyDevice;
static PFN_vkGetDeviceQueue pfn_vkGetDeviceQueue;
static PFN_vkCreateCommandPool pfn_vkCreateCommandPool;
static PFN_vkDestroyCommandPool pfn_vkDestroyCommandPool;
static PFN_vkCreateDescriptorSetLayout pfn_vkCreateDescriptorSetLayout;
static PFN_vkDestroyDescriptorSetLayout pfn_vkDestroyDescriptorSetLayout;
static PFN_vkCreatePipelineLayout pfn_vkCreatePipelineLayout;
static PFN_vkDestroyPipelineLayout pfn_vkDestroyPipelineLayout;
static PFN_vkCreateDescriptorPool pfn_vkCreateDescriptorPool;
static PFN_vkDestroyDescriptorPool pfn_vkDestroyDescriptorPool;
static PFN_vkResetDescriptorPool pfn_vkResetDescriptorPool;
static PFN_vkCreateShaderModule pfn_vkCreateShaderModule;
static PFN_vkDestroyShaderModule pfn_vkDestroyShaderModule;
static PFN_vkCreateComputePipelines pfn_vkCreateComputePipelines;
static PFN_vkDestroyPipeline pfn_vkDestroyPipeline;
static PFN_vkCreateBuffer pfn_vkCreateBuffer;
static PFN_vkDestroyBuffer pfn_vkDestroyBuffer;
static PFN_vkGetBufferMemoryRequirements pfn_vkGetBufferMemoryRequirements;
static PFN_vkAllocateMemory pfn_vkAllocateMemory;
static PFN_vkFreeMemory pfn_vkFreeMemory;
static PFN_vkBindBufferMemory pfn_vkBindBufferMemory;
static PFN_vkMapMemory pfn_vkMapMemory;
static PFN_vkUnmapMemory pfn_vkUnmapMemory;
static PFN_vkAllocateCommandBuffers pfn_vkAllocateCommandBuffers;
static PFN_vkFreeCommandBuffers pfn_vkFreeCommandBuffers;
static PFN_vkBeginCommandBuffer pfn_vkBeginCommandBuffer;
static PFN_vkEndCommandBuffer pfn_vkEndCommandBuffer;
static PFN_vkCreateFence pfn_vkCreateFence;
static PFN_vkDestroyFence pfn_vkDestroyFence;
static PFN_vkQueueSubmit pfn_vkQueueSubmit;
static PFN_vkWaitForFences pfn_vkWaitForFences;
static PFN_vkAllocateDescriptorSets pfn_vkAllocateDescriptorSets;
static PFN_vkUpdateDescriptorSets pfn_vkUpdateDescriptorSets;
static PFN_vkCmdBindPipeline pfn_vkCmdBindPipeline;
static PFN_vkCmdBindDescriptorSets pfn_vkCmdBindDescriptorSets;
static PFN_vkCmdPushConstants pfn_vkCmdPushConstants;
static PFN_vkCmdDispatch pfn_vkCmdDispatch;
static PFN_vkCmdPipelineBarrier pfn_vkCmdPipelineBarrier;

#define VK_LOAD_GLOBAL(name) \
    do { pfn_##name = (PFN_##name)dlsym(gCtx.vulkanLib, #name); if (!pfn_##name) return 0; } while (0)
#define VK_LOAD_INSTANCE(name) \
    do { pfn_##name = (PFN_##name)pfn_vkGetInstanceProcAddr(gCtx.instance, #name); if (!pfn_##name) return 0; } while (0)
#define VK_LOAD_DEVICE(name) \
    do { pfn_##name = (PFN_##name)pfn_vkGetDeviceProcAddr(gCtx.device, #name); if (!pfn_##name) return 0; } while (0)

static int vkLoadGlobalFns(void) {
    if (gCtx.vulkanLib) return 1;
    gCtx.vulkanLib = dlopen("libvulkan.so", RTLD_NOW | RTLD_LOCAL);
    if (!gCtx.vulkanLib) return 0;
    VK_LOAD_GLOBAL(vkGetInstanceProcAddr);
    VK_LOAD_GLOBAL(vkCreateInstance);
    pfn_vkGetDeviceProcAddr = (PFN_vkGetDeviceProcAddr)dlsym(gCtx.vulkanLib, "vkGetDeviceProcAddr");
    if (!pfn_vkGetDeviceProcAddr) return 0;
    return 1;
}

static int vkLoadInstanceFns(void) {
    VK_LOAD_INSTANCE(vkDestroyInstance);
    VK_LOAD_INSTANCE(vkEnumeratePhysicalDevices);
    VK_LOAD_INSTANCE(vkGetPhysicalDeviceQueueFamilyProperties);
    VK_LOAD_INSTANCE(vkGetPhysicalDeviceMemoryProperties);
    VK_LOAD_INSTANCE(vkCreateDevice);
    return 1;
}

static int vkLoadDeviceFns(void) {
    VK_LOAD_DEVICE(vkDestroyDevice);
    VK_LOAD_DEVICE(vkGetDeviceQueue);
    VK_LOAD_DEVICE(vkCreateCommandPool);
    VK_LOAD_DEVICE(vkDestroyCommandPool);
    VK_LOAD_DEVICE(vkCreateDescriptorSetLayout);
    VK_LOAD_DEVICE(vkDestroyDescriptorSetLayout);
    VK_LOAD_DEVICE(vkCreatePipelineLayout);
    VK_LOAD_DEVICE(vkDestroyPipelineLayout);
    VK_LOAD_DEVICE(vkCreateDescriptorPool);
    VK_LOAD_DEVICE(vkDestroyDescriptorPool);
    VK_LOAD_DEVICE(vkResetDescriptorPool);
    VK_LOAD_DEVICE(vkCreateShaderModule);
    VK_LOAD_DEVICE(vkDestroyShaderModule);
    VK_LOAD_DEVICE(vkCreateComputePipelines);
    VK_LOAD_DEVICE(vkDestroyPipeline);
    VK_LOAD_DEVICE(vkCreateBuffer);
    VK_LOAD_DEVICE(vkDestroyBuffer);
    VK_LOAD_DEVICE(vkGetBufferMemoryRequirements);
    VK_LOAD_DEVICE(vkAllocateMemory);
    VK_LOAD_DEVICE(vkFreeMemory);
    VK_LOAD_DEVICE(vkBindBufferMemory);
    VK_LOAD_DEVICE(vkMapMemory);
    VK_LOAD_DEVICE(vkUnmapMemory);
    VK_LOAD_DEVICE(vkAllocateCommandBuffers);
    VK_LOAD_DEVICE(vkFreeCommandBuffers);
    VK_LOAD_DEVICE(vkBeginCommandBuffer);
    VK_LOAD_DEVICE(vkEndCommandBuffer);
    VK_LOAD_DEVICE(vkCreateFence);
    VK_LOAD_DEVICE(vkDestroyFence);
    VK_LOAD_DEVICE(vkQueueSubmit);
    VK_LOAD_DEVICE(vkWaitForFences);
    VK_LOAD_DEVICE(vkAllocateDescriptorSets);
    VK_LOAD_DEVICE(vkUpdateDescriptorSets);
    VK_LOAD_DEVICE(vkCmdBindPipeline);
    VK_LOAD_DEVICE(vkCmdBindDescriptorSets);
    VK_LOAD_DEVICE(vkCmdPushConstants);
    VK_LOAD_DEVICE(vkCmdDispatch);
    VK_LOAD_DEVICE(vkCmdPipelineBarrier);
    return 1;
}

static uint32_t vkFindMemoryType(uint32_t typeBits, VkMemoryPropertyFlags props) {
    VkPhysicalDeviceMemoryProperties memProps;
    pfn_vkGetPhysicalDeviceMemoryProperties(gCtx.physicalDevice, &memProps);
    for (uint32_t i = 0; i < memProps.memoryTypeCount; i++) {
        if ((typeBits & (1u << i)) && (memProps.memoryTypes[i].propertyFlags & props) == props) {
            return i;
        }
    }
    return UINT32_MAX;
}

static int vkCreateHostBuffer(VkDeviceSize size, HostBuffer* out) {
    memset(out, 0, sizeof(*out));
    out->size = size;

    VkBufferCreateInfo bufferInfo = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size = size,
            .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };
    if (pfn_vkCreateBuffer(gCtx.device, &bufferInfo, NULL, &out->buffer) != VK_SUCCESS) {
        return 0;
    }

    VkMemoryRequirements memReq;
    pfn_vkGetBufferMemoryRequirements(gCtx.device, out->buffer, &memReq);
    uint32_t typeIndex = vkFindMemoryType(memReq.memoryTypeBits,
                                          VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                          VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if (typeIndex == UINT32_MAX) {
        pfn_vkDestroyBuffer(gCtx.device, out->buffer, NULL);
        memset(out, 0, sizeof(*out));
        return 0;
    }

    VkMemoryAllocateInfo allocInfo = {
            .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .allocationSize = memReq.size,
            .memoryTypeIndex = typeIndex,
    };
    if (pfn_vkAllocateMemory(gCtx.device, &allocInfo, NULL, &out->memory) != VK_SUCCESS) {
        pfn_vkDestroyBuffer(gCtx.device, out->buffer, NULL);
        memset(out, 0, sizeof(*out));
        return 0;
    }

    pfn_vkBindBufferMemory(gCtx.device, out->buffer, out->memory, 0);
    if (pfn_vkMapMemory(gCtx.device, out->memory, 0, memReq.size, 0, &out->mapped) != VK_SUCCESS) {
        pfn_vkFreeMemory(gCtx.device, out->memory, NULL);
        pfn_vkDestroyBuffer(gCtx.device, out->buffer, NULL);
        memset(out, 0, sizeof(*out));
        return 0;
    }

    return 1;
}

static void vkDestroyHostBuffer(HostBuffer* b) {
    if (b->mapped) pfn_vkUnmapMemory(gCtx.device, b->memory);
    if (b->memory) pfn_vkFreeMemory(gCtx.device, b->memory, NULL);
    if (b->buffer) pfn_vkDestroyBuffer(gCtx.device, b->buffer, NULL);
    memset(b, 0, sizeof(*b));
}

static VkShaderModule vkMakeShaderModule(const unsigned char* data, size_t len) {
    VkShaderModule module = VK_NULL_HANDLE;
    VkShaderModuleCreateInfo info = {
            .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            .codeSize = len,
            .pCode = (const uint32_t*)data,
    };
    if (pfn_vkCreateShaderModule(gCtx.device, &info, NULL, &module) != VK_SUCCESS) {
        return VK_NULL_HANDLE;
    }
    return module;
}

static VkPipeline vkMakePipeline(const unsigned char* data, size_t len) {
    VkShaderModule module = vkMakeShaderModule(data, len);
    if (!module) return VK_NULL_HANDLE;

    VkPipelineShaderStageCreateInfo stage = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_COMPUTE_BIT,
            .module = module,
            .pName = "main",
    };
    VkComputePipelineCreateInfo info = {
            .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
            .stage = stage,
            .layout = gCtx.pipelineLayout,
    };

    VkPipeline pipeline = VK_NULL_HANDLE;
    VkResult res = pfn_vkCreateComputePipelines(gCtx.device, VK_NULL_HANDLE, 1, &info, NULL, &pipeline);
    pfn_vkDestroyShaderModule(gCtx.device, module, NULL);
    return res == VK_SUCCESS ? pipeline : VK_NULL_HANDLE;
}

static int vkInitContextLocked(void) {
    if (gCtx.initialized) return 1;
    if (gCtx.failed) return 0;

    VkApplicationInfo appInfo = {
            .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
            .pApplicationName = "QmBlurView",
            .applicationVersion = 1,
            .pEngineName = "QmBlurView",
            .engineVersion = 1,
            .apiVersion = VK_API_VERSION_1_0,
    };
    VkInstanceCreateInfo instInfo = {
            .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
            .pApplicationInfo = &appInfo,
    };
    if (!vkLoadGlobalFns()) goto fail;
    if (pfn_vkCreateInstance(&instInfo, NULL, &gCtx.instance) != VK_SUCCESS) goto fail;
    if (!vkLoadInstanceFns()) goto fail;

    uint32_t deviceCount = 0;
    if (pfn_vkEnumeratePhysicalDevices(gCtx.instance, &deviceCount, NULL) != VK_SUCCESS || deviceCount == 0) goto fail;
    VkPhysicalDevice devices[8];
    if (deviceCount > 8) deviceCount = 8;
    if (pfn_vkEnumeratePhysicalDevices(gCtx.instance, &deviceCount, devices) != VK_SUCCESS) goto fail;

    for (uint32_t d = 0; d < deviceCount && !gCtx.physicalDevice; d++) {
        uint32_t qCount = 0;
        pfn_vkGetPhysicalDeviceQueueFamilyProperties(devices[d], &qCount, NULL);
        VkQueueFamilyProperties qProps[16];
        if (qCount > 16) qCount = 16;
        pfn_vkGetPhysicalDeviceQueueFamilyProperties(devices[d], &qCount, qProps);
        for (uint32_t q = 0; q < qCount; q++) {
            if (qProps[q].queueFlags & VK_QUEUE_COMPUTE_BIT) {
                gCtx.physicalDevice = devices[d];
                gCtx.queueFamilyIndex = q;
                break;
            }
        }
    }
    if (!gCtx.physicalDevice) goto fail;

    float priority = 1.0f;
    VkDeviceQueueCreateInfo queueInfo = {
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .queueFamilyIndex = gCtx.queueFamilyIndex,
            .queueCount = 1,
            .pQueuePriorities = &priority,
    };
    VkDeviceCreateInfo devInfo = {
            .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
            .queueCreateInfoCount = 1,
            .pQueueCreateInfos = &queueInfo,
    };
    if (pfn_vkCreateDevice(gCtx.physicalDevice, &devInfo, NULL, &gCtx.device) != VK_SUCCESS) goto fail;
    if (!vkLoadDeviceFns()) goto fail;
    pfn_vkGetDeviceQueue(gCtx.device, gCtx.queueFamilyIndex, 0, &gCtx.queue);

    VkCommandPoolCreateInfo poolInfo = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
            .queueFamilyIndex = gCtx.queueFamilyIndex,
    };
    if (pfn_vkCreateCommandPool(gCtx.device, &poolInfo, NULL, &gCtx.commandPool) != VK_SUCCESS) goto fail;

    VkDescriptorSetLayoutBinding bindings[2];
    memset(bindings, 0, sizeof(bindings));
    bindings[0].binding = 0;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    bindings[1].binding = 1;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[1].descriptorCount = 1;
    bindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    VkDescriptorSetLayoutCreateInfo setLayoutInfo = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .bindingCount = 2,
            .pBindings = bindings,
    };
    if (pfn_vkCreateDescriptorSetLayout(gCtx.device, &setLayoutInfo, NULL, &gCtx.descriptorSetLayout) != VK_SUCCESS) goto fail;

    VkPushConstantRange pushRange = {
            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
            .offset = 0,
            .size = sizeof(Push),
    };
    VkPipelineLayoutCreateInfo layoutInfo = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .setLayoutCount = 1,
            .pSetLayouts = &gCtx.descriptorSetLayout,
            .pushConstantRangeCount = 1,
            .pPushConstantRanges = &pushRange,
    };
    if (pfn_vkCreatePipelineLayout(gCtx.device, &layoutInfo, NULL, &gCtx.pipelineLayout) != VK_SUCCESS) goto fail;

    VkDescriptorPoolSize poolSize = {
            .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .descriptorCount = 64,
    };
    VkDescriptorPoolCreateInfo poolCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
            .maxSets = 32,
            .poolSizeCount = 1,
            .pPoolSizes = &poolSize,
    };
    if (pfn_vkCreateDescriptorPool(gCtx.device, &poolCreateInfo, NULL, &gCtx.descriptorPool) != VK_SUCCESS) goto fail;

    gCtx.pipelineDown4 = vkMakePipeline(dual_kawase_down4_spv, dual_kawase_down4_spv_len);
    gCtx.pipelineDown5 = vkMakePipeline(dual_kawase_down5_spv, dual_kawase_down5_spv_len);
    gCtx.pipelineUp8 = vkMakePipeline(dual_kawase_up8_spv, dual_kawase_up8_spv_len);
    if (!gCtx.pipelineDown4 || !gCtx.pipelineDown5 || !gCtx.pipelineUp8) goto fail;

    gCtx.initialized = 1;
    return 1;

fail:
    VKLOGE("Failed to initialize Vulkan blur context");
    gCtx.failed = 1;
    return 0;
}

static int vkDispatchPass(VkCommandBuffer cmd, VkPipeline pipeline, HostBuffer* src, HostBuffer* dst,
                          int srcW, int srcH, int dstW, int dstH, int step) {
    VkDescriptorSetLayout layouts[] = { gCtx.descriptorSetLayout };
    VkDescriptorSetAllocateInfo allocInfo = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .descriptorPool = gCtx.descriptorPool,
            .descriptorSetCount = 1,
            .pSetLayouts = layouts,
    };
    VkDescriptorSet set = VK_NULL_HANDLE;
    if (pfn_vkAllocateDescriptorSets(gCtx.device, &allocInfo, &set) != VK_SUCCESS) return 0;

    VkDescriptorBufferInfo srcInfo = { .buffer = src->buffer, .offset = 0, .range = src->size };
    VkDescriptorBufferInfo dstInfo = { .buffer = dst->buffer, .offset = 0, .range = dst->size };
    VkWriteDescriptorSet writes[2];
    memset(writes, 0, sizeof(writes));
    writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[0].dstSet = set;
    writes[0].dstBinding = 0;
    writes[0].descriptorCount = 1;
    writes[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    writes[0].pBufferInfo = &srcInfo;
    writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[1].dstSet = set;
    writes[1].dstBinding = 1;
    writes[1].descriptorCount = 1;
    writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    writes[1].pBufferInfo = &dstInfo;
    pfn_vkUpdateDescriptorSets(gCtx.device, 2, writes, 0, NULL);

    Push push = { srcW, srcH, dstW, dstH, step };
    pfn_vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
    pfn_vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, gCtx.pipelineLayout, 0, 1, &set, 0, NULL);
    pfn_vkCmdPushConstants(cmd, gCtx.pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(push), &push);
    pfn_vkCmdDispatch(cmd, (uint32_t)((dstW + 15) / 16), (uint32_t)((dstH + 15) / 16), 1);

    VkBufferMemoryBarrier barriers[2];
    memset(barriers, 0, sizeof(barriers));
    for (int i = 0; i < 2; i++) {
        barriers[i].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
        barriers[i].srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        barriers[i].dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
        barriers[i].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barriers[i].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    }
    barriers[0].buffer = src->buffer;
    barriers[0].size = src->size;
    barriers[1].buffer = dst->buffer;
    barriers[1].size = dst->size;
    pfn_vkCmdPipelineBarrier(cmd,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         0, 0, NULL, 2, barriers, 0, NULL);
    return 1;
}

int dualKawaseV2_vulkan_rgba(unsigned char* rgba, int w, int h, int radius, int rounds) {
    if (!rgba || w <= 0 || h <= 0 || radius <= 0) return 0;

    pthread_mutex_lock(&gVkMutex);
    if (!vkInitContextLocked()) {
        pthread_mutex_unlock(&gVkMutex);
        return 0;
    }

    int levels = rounds + 1;
    if (levels < 2) levels = 2;
    if (levels > 4) levels = 4;
    int step = radius < 1 ? 1 : (radius > 32 ? 32 : radius);

    HostBuffer bufs[4];
    int ws[4];
    int hs[4];
    memset(bufs, 0, sizeof(bufs));
    for (int i = 0; i < levels; i++) {
        ws[i] = w >> i; if (ws[i] < 1) ws[i] = 1;
        hs[i] = h >> i; if (hs[i] < 1) hs[i] = 1;
        if (!vkCreateHostBuffer((VkDeviceSize)ws[i] * (VkDeviceSize)hs[i] * 4, &bufs[i])) goto fail;
    }
    memcpy(bufs[0].mapped, rgba, (size_t)w * (size_t)h * 4);

    VkCommandBufferAllocateInfo alloc = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .commandPool = gCtx.commandPool,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = 1,
    };
    VkCommandBuffer cmd = VK_NULL_HANDLE;
    if (pfn_vkAllocateCommandBuffers(gCtx.device, &alloc, &cmd) != VK_SUCCESS) goto fail;

    VkCommandBufferBeginInfo begin = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };
    pfn_vkBeginCommandBuffer(cmd, &begin);

    if (!vkDispatchPass(cmd, gCtx.pipelineDown4, &bufs[0], &bufs[1], ws[0], hs[0], ws[1], hs[1], step)) goto fail_cmd;
    for (int i = 1; i < levels - 1; i++) {
        if (!vkDispatchPass(cmd, gCtx.pipelineDown5, &bufs[i], &bufs[i + 1], ws[i], hs[i], ws[i + 1], hs[i + 1], step)) goto fail_cmd;
    }
    for (int i = levels - 2; i >= 0; i--) {
        if (!vkDispatchPass(cmd, gCtx.pipelineUp8, &bufs[i + 1], &bufs[i], ws[i + 1], hs[i + 1], ws[i], hs[i], step)) goto fail_cmd;
    }

    if (pfn_vkEndCommandBuffer(cmd) != VK_SUCCESS) goto fail_cmd;

    VkFenceCreateInfo fenceInfo = { .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
    VkFence fence = VK_NULL_HANDLE;
    if (pfn_vkCreateFence(gCtx.device, &fenceInfo, NULL, &fence) != VK_SUCCESS) goto fail_cmd;

    VkSubmitInfo submit = {
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .commandBufferCount = 1,
            .pCommandBuffers = &cmd,
    };
    if (pfn_vkQueueSubmit(gCtx.queue, 1, &submit, fence) != VK_SUCCESS) {
        pfn_vkDestroyFence(gCtx.device, fence, NULL);
        goto fail_cmd;
    }
    if (pfn_vkWaitForFences(gCtx.device, 1, &fence, VK_TRUE, UINT64_MAX) != VK_SUCCESS) {
        pfn_vkDestroyFence(gCtx.device, fence, NULL);
        goto fail_cmd;
    }
    pfn_vkDestroyFence(gCtx.device, fence, NULL);

    memcpy(rgba, bufs[0].mapped, (size_t)w * (size_t)h * 4);

    pfn_vkFreeCommandBuffers(gCtx.device, gCtx.commandPool, 1, &cmd);
    for (int i = 0; i < levels; i++) vkDestroyHostBuffer(&bufs[i]);
    pfn_vkResetDescriptorPool(gCtx.device, gCtx.descriptorPool, 0);
    pthread_mutex_unlock(&gVkMutex);
    return 1;

fail_cmd:
    if (cmd) pfn_vkFreeCommandBuffers(gCtx.device, gCtx.commandPool, 1, &cmd);
fail:
    for (int i = 0; i < levels; i++) vkDestroyHostBuffer(&bufs[i]);
    if (gCtx.descriptorPool) pfn_vkResetDescriptorPool(gCtx.device, gCtx.descriptorPool, 0);
    pthread_mutex_unlock(&gVkMutex);
    return 0;
}

