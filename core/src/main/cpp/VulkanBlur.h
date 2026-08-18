#pragma once

#ifdef __cplusplus
extern "C" {
#endif

int vulkan_kawase_blur_rgba(unsigned char* rgba, int w, int h, int stride, float radius, int rounds);

#ifdef __cplusplus
}
#endif
