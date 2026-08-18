cmake_minimum_required(VERSION 3.22.1)
project("QmBlur" LANGUAGES C CXX)

add_library(
        QmBlur
        SHARED
        BlurNative.c
        VulkanBlur.cpp
        KawaseBlur.cpp
        VulkanImage.cpp
        VulkanBuffer.cpp
)

target_include_directories(QmBlur PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_BINARY_DIR}
)
target_compile_features(QmBlur PRIVATE cxx_std_17)
target_compile_options(QmBlur PRIVATE -O3)
target_compile_options(QmBlur PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions -fno-rtti>)

find_library(log-lib log)
find_library(jni-graphics-lib jnigraphics)

# Vulkan stubs start at API 24; minSdk 21 sysroot has none. Link the 24 stub
# (DT_NEEDED libvulkan.so). Runtime still loads the device ICD.
if(ANDROID_ABI STREQUAL "arm64-v8a")
    set(_vk_triple aarch64-linux-android)
elseif(ANDROID_ABI STREQUAL "armeabi-v7a")
    set(_vk_triple arm-linux-androideabi)
elseif(ANDROID_ABI STREQUAL "x86")
    set(_vk_triple i686-linux-android)
elseif(ANDROID_ABI STREQUAL "x86_64")
    set(_vk_triple x86_64-linux-android)
endif()
if(NOT DEFINED ANDROID_NDK_HOST_SYSTEM_NAME)
    set(ANDROID_NDK_HOST_SYSTEM_NAME linux-x86_64)
endif()
set(_vk_stub "${ANDROID_NDK}/toolchains/llvm/prebuilt/${ANDROID_NDK_HOST_SYSTEM_NAME}/sysroot/usr/lib/${_vk_triple}/24/libvulkan.so")
if(NOT EXISTS "${_vk_stub}")
    message(FATAL_ERROR "Vulkan stub not found: ${_vk_stub}")
endif()

target_link_libraries(
        QmBlur
        ${log-lib}
        ${jni-graphics-lib}
        ${_vk_stub}
)

target_link_options(${CMAKE_PROJECT_NAME} PRIVATE "-Wl,-z,max-page-size=16384")

set(_glslc "${ANDROID_NDK}/shader-tools/${ANDROID_NDK_HOST_SYSTEM_NAME}/glslc")
if(NOT EXISTS "${_glslc}")
    set(_glslc "${ANDROID_NDK}/shader-tools/linux-x86_64/glslc")
endif()
if(NOT EXISTS "${_glslc}")
    message(FATAL_ERROR "glslc not found under ${ANDROID_NDK}/shader-tools")
endif()

set(_shader_incs "")
macro(embed_comp NAME)
    set(_src "${CMAKE_CURRENT_SOURCE_DIR}/shaders/${NAME}.comp")
    set(_inc "${CMAKE_CURRENT_BINARY_DIR}/${NAME}_comp_spv.inc")
    add_custom_command(
            OUTPUT "${_inc}"
            COMMAND "${_glslc}" -fshader-stage=compute -mfmt=c "${_src}" -o "${_inc}"
            DEPENDS "${_src}"
            COMMENT "SPIR-V ${NAME}.comp"
            VERBATIM
    )
    list(APPEND _shader_incs "${_inc}")
endmacro()

embed_comp(kawase_down)
embed_comp(kawase_up)
embed_comp(kawase_draw)
set_source_files_properties(KawaseBlur.cpp PROPERTIES OBJECT_DEPENDS "${_shader_incs}")
add_custom_target(shaders DEPENDS ${_shader_incs})
add_dependencies(QmBlur shaders)
