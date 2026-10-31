# Copyright 2021 The Lynx Authors. All rights reserved.
# Licensed under the Apache License Version 2.0 that can be found in the
# LICENSE file in the root directory of this source tree.

include(GoogleTest)

enable_testing()

if (${SKITY_UNIFIED_LIB})
    # Unified-lib builds replace skity_unit_test — its legacy-API test sources
    # cannot link against a library that exports only the C ABI — with this
    # dedicated binary: the C ABI tests that need no legacy surface (the
    # header-only wrapper tests join here once they land).
    add_executable(skity_wrapper_unit_test
        capi/text_delegate_c_test.cc
        capi/serialization_c_test.cc
        capi/path_2d_c_test.cc
        capi/path_2d_c_smoke.c
    )

    target_include_directories(skity_wrapper_unit_test PRIVATE
        ${CMAKE_SOURCE_DIR}/module/capi/include)

    if (${SKITY_VK_BACKEND})
        target_sources(skity_wrapper_unit_test PRIVATE
            capi/vulkan_c_test.cc
        )
        target_include_directories(skity_wrapper_unit_test PRIVATE
            ${CMAKE_SOURCE_DIR}/third_party/Vulkan-Headers/include
        )
    endif()

    target_compile_definitions(skity_wrapper_unit_test PRIVATE
        -DSKITY_FONT_DIR="${CMAKE_SOURCE_DIR}/test/")
    if (CMAKE_SYSTEM_NAME MATCHES "Windows")
        target_compile_definitions(skity_wrapper_unit_test PRIVATE
            -DSKITY_TEST_FONT_ROOT="${CMAKE_SOURCE_DIR}/test/fonts/resources"
        )
    endif()

    if (MSVC)
        target_compile_options(skity_wrapper_unit_test PUBLIC /EHsc /GR-)
    else()
        target_compile_options(skity_wrapper_unit_test PUBLIC -fno-rtti)
    endif()

    target_link_libraries(skity_wrapper_unit_test PUBLIC skity::capi gmock_main)

    gtest_add_tests(skity_wrapper_unit_test "" AUTO)

    # the legacy-API binaries below cannot link against the unified library
    return()
endif()

# Test case list
add_executable(skity_unit_test
    base/lru_cache_test.cc
    effect/color_filter_test.cc
    effect/image_filter_test.cc
    effect/mask_filter_test.cc
    effect/shader_test.cc
    geometry/geometry_test.cc
    geometry/math_test.cc
    geometry/matrix_test.cc
    geometry/quaternion_test.cc
    geometry/rect_test.cc
    geometry/rrect_test.cc
    geometry/scalar_test.cc
    geometry/vector_test.cc
    graphic/bitmap_test.cc
    graphic/color_test.cc
    graphic/image_test.cc
    graphic/path_measure_test.cc
    graphic/path_2d_test.cc
    graphic/path_test.cc
    graphic/texture_format_test.cc
    io/data_test.cc
    io/pixmap_test.cc
    render/canvas_state_test.cc
    render/sw_canvas_test.cc
    render/sw_raster_test.cc
    render/hw/hw_buffer_layout_test.cc
    render/hw/coverage_aa_line_encoder_test.cc
    render/hw/coverage_aa_path_tiler_test.cc
    render/hw/hw_pipeline_key_test.cc
    render/hw/hw_sub_layer_discard_test.cc
    render/hw/precompile_test.cc
    render/hw/gaussian_bounds_test.cc
    render/hw/dst_read_strategy_test.cc
    render/hw/hw_blend_plan_test.cc
    render/hw/hw_texture_copy_info_test.cc
    render/hw/draw/hw_wgsl_shader_writer_test.cc
    wgx/wgx_backend_name_resolution_test.cc
    wgx/wgx_parser_test.cc
    wgx/wgx_resolver_test.cc
    wgx/wgx_uniform_layout_test.cc
    render/resource_cache_test.cc
    render/shape_test.cc
    recorder/display_list_test.cc
    recorder/display_list_region_test.cc
    recorder/display_list_rtree_test.cc
    text/atlas_glyph_test.cc
    text/glyph_image_test.cc
    text/glyph_bitmap_raster_options_test.cc
    text/glyph_raster_test.cc
    text/font_path_visit_test.cc
    text/scaler_context_cache_test.cc
    text/text_blob_test.cc
    text/text_run_test.cc
    text/text_test.cc
    text/transformed_mask_glyph_run_test.cc
    utils/arena_allocator_test.cc
    utils/array_list_test.cc
)

if(${SKITY_GL_BACKEND})
    target_include_directories(skity_unit_test PRIVATE
        ${CMAKE_SOURCE_DIR}/third_party/glad/include
    )
    target_sources(skity_unit_test PRIVATE
        gpu/gl/gpu_driver_info_gl_test.cc
        gpu/gl/gpu_render_pass_gl_test.cc
    )
endif()

if(CMAKE_SYSTEM_NAME MATCHES "Windows")
    target_sources(skity_unit_test PRIVATE
        base/platform/win/str_conversion_test.cc
        text/a8_mask_gamma_test.cc
        text/dwrite_stability_test.cc
    )
    target_compile_definitions(skity_unit_test PRIVATE
        -DSKITY_TEST_FONT_ROOT="${CMAKE_SOURCE_DIR}/test/fonts/resources"
    )
endif()

target_compile_definitions(skity_unit_test PRIVATE
    -DSKITY_FONT_DIR="${CMAKE_SOURCE_DIR}/test/")

# Place the cases that depend on specific fonts here.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux" OR (CMAKE_SYSTEM_NAME STREQUAL "Darwin" AND NOT SKITY_CT_FONT))
  target_compile_definitions(skity_unit_test PRIVATE
      -DFREETYPE_STATIC_LIB=1)
  target_include_directories(skity_unit_test PRIVATE
      ${CMAKE_SOURCE_DIR}/third_party/freetype2/include)

  target_sources(skity_unit_test PRIVATE text/typeface_test.cc)
  if(SKITY_FONTCONFIG)
    find_package(Fontconfig 2.13.1 REQUIRED)
    target_link_libraries(skity_unit_test PRIVATE Fontconfig::Fontconfig)
    target_sources(skity_unit_test PRIVATE
      text/font_manager_fontconfig_test.cc
      ${CMAKE_SOURCE_DIR}/src/text/ports/test/font_manager_test.cc)
    set_property(SOURCE text/font_manager_fontconfig_test.cc APPEND PROPERTY
      COMPILE_DEFINITIONS
      SKITY_TEST_FONTCONFIG_FIXTURE_ROOT="${CMAKE_CURRENT_SOURCE_DIR}/text/fontconfig")
    target_compile_definitions(skity_unit_test PRIVATE
      SKITY_TEST_FONT_MANAGER_NO_DEFAULT=1
      SKITY_TEST_FONT_ROOT="${CMAKE_SOURCE_DIR}/test/fonts/resources")
  endif()
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Darwin" AND SKITY_CT_FONT)
  target_sources(skity_unit_test PRIVATE text/coretext_typeface_test.cc)

  if(SKITY_MTL_BACKEND)
    target_sources(skity_unit_test PRIVATE render/hw/save_layer_pixel_grid_test.mm)
    target_sources(skity_unit_test PRIVATE render/hw/color_glyph_threshold_test.mm)

    set_source_files_properties(render/hw/color_glyph_threshold_test.mm
        PROPERTIES COMPILE_FLAGS "-fobjc-arc")
    set_source_files_properties(render/hw/save_layer_pixel_grid_test.mm
        PROPERTIES COMPILE_FLAGS "-fobjc-arc")
    target_link_libraries(skity_unit_test PRIVATE "-framework AppKit" "-framework Metal")
  endif()
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Darwin" AND SKITY_MTL_BACKEND)
  target_sources(skity_unit_test PRIVATE render/hw/text_atlas_sampling_test.mm)
  set_source_files_properties(render/hw/text_atlas_sampling_test.mm
      PROPERTIES COMPILE_FLAGS "-fobjc-arc")
  target_link_libraries(skity_unit_test PRIVATE "-framework Metal")
endif()

target_include_directories(skity_unit_test
    PUBLIC
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/module/wgx
)

target_link_libraries(skity_unit_test
    PUBLIC
    skity::skity
    gmock_main
)

if (${SKITY_CODEC_MODULE})
    target_compile_definitions(skity_unit_test PRIVATE
        SKITY_ENABLE_CODEC_BMP=$<BOOL:${SKITY_ENABLE_CODEC_BMP}>
    )
    target_sources(skity_unit_test
        PUBLIC
        codec/bmp_codec_test.cc
        codec/codec_scale_test.cc
        codec/jpeg_codec_test.cc
        codec/png_codec_test.cc
        codec/gif_codec_test.cc
        codec/webp_codec_test.cc
    )

    target_compile_definitions(skity_unit_test PRIVATE -DSKITY_TEST_PNG_FILE="${CMAKE_SOURCE_DIR}/example/images/firefox_64.png")
    target_compile_definitions(skity_unit_test PRIVATE -DSKITY_TEST_JPEG_FILE="${CMAKE_SOURCE_DIR}/example/images/image1.jpg")
    target_compile_definitions(skity_unit_test PRIVATE -DSKITY_TEST_CORRUPT_JPEG_FILE="${CMAKE_SOURCE_DIR}/example/images/corrupted.jpg")
    target_compile_definitions(skity_unit_test PRIVATE -DSKITY_TEST_MF_GIF_FILE="${CMAKE_SOURCE_DIR}/example/images/alphabetAnim.gif")
    target_compile_definitions(skity_unit_test PRIVATE -DSKITY_TEST_SF_GIF_FILE="${CMAKE_SOURCE_DIR}/example/images/color_wheel.gif")
    target_compile_definitions(skity_unit_test PRIVATE -DSKITY_TEST_WEBP_FILE="${CMAKE_SOURCE_DIR}/example/images/blendBG.webp")
    target_compile_definitions(skity_unit_test PRIVATE -DSKITY_TEST_SF_WEBP_FILE="${CMAKE_SOURCE_DIR}/example/images/color_wheel.webp")
    target_compile_definitions(skity_unit_test PRIVATE -DSKITY_TEST_SINGLE_ANIM_WEBP_FILE="${CMAKE_SOURCE_DIR}/example/images/single_frame_anim.webp")

    target_link_libraries(skity_unit_test PUBLIC skity::codec)
endif()

# C API wrapper tests (CPU-only; replay targets are recording canvases).
if (${SKITY_CAPI_MODULE})
    target_sources(skity_unit_test PRIVATE
        capi/bridge_c_test.cc
        capi/gaussian_mask_factory_test.cc
        capi/recorder_c_test.cc
        capi/text_delegate_c_test.cc
        capi/serialization_c_test.cc
        capi/path_2d_c_test.cc
        capi/path_2d_c_smoke.c
    )
    if (${SKITY_VK_BACKEND})
        target_sources(skity_unit_test PRIVATE
            capi/vulkan_c_test.cc
        )
        target_include_directories(skity_unit_test PRIVATE
            ${CMAKE_SOURCE_DIR}/third_party/Vulkan-Headers/include
        )
    endif()

    target_include_directories(skity_unit_test PRIVATE
        ${CMAKE_SOURCE_DIR}/module/capi/include
    )

    target_link_libraries(skity_unit_test PUBLIC skity::capi)
endif()

if (${SKITY_IO_MODULE})
    target_sources(skity_unit_test PRIVATE io/mask_filter_serialization_test.cc io/core_bridge_test.cc)
    target_include_directories(skity_unit_test PRIVATE ${SKITY_ROOT}/third_party/glm)
    target_include_directories(skity_unit_test PRIVATE
        ${CMAKE_SOURCE_DIR}/module/io
    )
    target_link_libraries(skity_unit_test PUBLIC skity::io)
    if(NOT SKITY_CPP_ALLOCATION_RECOVERY)
        target_link_libraries(skity_unit_test PRIVATE skity-capi-internal)
    endif()
endif()

# Match standard exception identity while retaining the legacy defaults.
if (MSVC)
    target_compile_options(skity_unit_test PUBLIC /EHsc /GR-)
else()
    if(SKITY_CPP_ALLOCATION_RECOVERY)
        target_compile_options(skity_unit_test PUBLIC -frtti)
    else()
        target_compile_options(skity_unit_test PUBLIC -fno-rtti)
    endif()
endif()

if (${SKITY_TEST_COVERAGE} AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    # Coverage flags
    target_compile_options(skity PRIVATE --coverage -O0 -g)
    target_link_options(skity PRIVATE --coverage)

    target_compile_options(wgsl-cross PRIVATE --coverage -O0 -g)
    target_link_options(wgsl-cross PRIVATE --coverage)

    if (${SKITY_CODEC_MODULE})
        target_compile_options(skity-codec PRIVATE --coverage -O0 -g)
        target_link_options(skity-codec PRIVATE --coverage)
    endif()

    target_compile_options(skity_unit_test PRIVATE --coverage -O0 -g)
    target_link_options(skity_unit_test PRIVATE --coverage)
elseif (${SKITY_TEST_TSAN} AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(skity PRIVATE
        -fsanitize=thread
        -fno-omit-frame-pointer
        -g
    )
    target_link_options(skity PRIVATE
        -fsanitize=thread
    )

    target_compile_options(skity_unit_test PRIVATE
        -fsanitize=thread
        -fno-omit-frame-pointer
        -g
    )

    target_link_options(skity_unit_test PRIVATE
        -fsanitize=thread
    )
endif()

target_link_libraries(skity_unit_test PRIVATE glm::glm-header-only)
target_link_libraries(skity_unit_test PRIVATE wgsl-cross)

if (${WGX_SPIRV_BACKEND})
    target_sources(skity_unit_test PRIVATE
        wgx/wgx_spirv_smoke_test.cc
        wgx/wgx_value_test.cc
    )
    target_compile_definitions(skity_unit_test PRIVATE
        -DWGX_SPIRV=1
        -DWGX_SPIRV_DUMP_DEFAULT_DIR="${CMAKE_BINARY_DIR}"
    )
    target_link_libraries(skity_unit_test PRIVATE
        SPIRV-Headers::SPIRV-Headers
    )
endif()

if(SKITY_ENABLE_FONT_HARNESS AND TARGET skity_font_harness)
    target_sources(skity_unit_test PRIVATE
        ${CMAKE_SOURCE_DIR}/harness/font/tests/unit/font_harness_self_test.cc
    )
    target_link_libraries(skity_unit_test PRIVATE skity_font_harness)
endif()

if (${SKITY_VK_BACKEND})
    set(SKITY_VK_TEST_LOADER_LIBRARIES)
    set(SKITY_VK_TEST_RPATH)
    if (${SKITY_VK_TEST_USE_SYSTEM_LOADER})
        find_package(Vulkan REQUIRED)
        list(APPEND SKITY_VK_TEST_LOADER_LIBRARIES Vulkan::Vulkan)
    elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        list(APPEND SKITY_VK_TEST_LOADER_LIBRARIES
            ${CMAKE_SOURCE_DIR}/third_party/libSwiftShader/libvulkan.so.1
        )
        set(SKITY_VK_TEST_RPATH "${CMAKE_SOURCE_DIR}/third_party/libSwiftShader")
    else()
        list(APPEND SKITY_VK_TEST_LOADER_LIBRARIES
            ${CMAKE_SOURCE_DIR}/third_party/libSwAngle/lib/libvk_swiftshader.dylib
        )
        set(SKITY_VK_TEST_RPATH "${CMAKE_SOURCE_DIR}/third_party/libSwAngle/lib")
    endif()

    add_executable(skity_vulkan_unit_test
        gpu/vk/gpu_context_vk_test.cc
        wgx/wgx_vulkan_pipeline_test.cc
    )

    target_include_directories(skity_vulkan_unit_test
        PUBLIC
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/module/wgx
    )
    if (${SKITY_VK_TEST_USE_SYSTEM_LOADER})
        target_compile_definitions(skity_vulkan_unit_test PRIVATE -DSKITY_VK_TEST_USE_SYSTEM_LOADER=1)
    endif()
    target_include_directories(skity_vulkan_unit_test PRIVATE
        ${CMAKE_SOURCE_DIR}/third_party/Vulkan-Headers/include
        ${CMAKE_SOURCE_DIR}/third_party/VulkanMemoryAllocator/include
    )
    target_link_libraries(skity_vulkan_unit_test PRIVATE
        skity::skity
        gmock_main
        glm::glm-header-only
        wgsl-cross
        ${SKITY_VK_TEST_LOADER_LIBRARIES}
    )
    if (NOT USE_THIRD_PARTY_ZLIB)
        target_link_libraries(skity_vulkan_unit_test PRIVATE z)
    else()
        target_link_libraries(skity_vulkan_unit_test PRIVATE ${SKITY_ZLIB_NAME})
        target_link_directories(skity_vulkan_unit_test PRIVATE ${CMAKE_BINARY_DIR}/third_party/zlib/lib)
        add_dependencies(skity_vulkan_unit_test zlib)
    endif()
    if (MSVC)
        target_compile_options(skity_vulkan_unit_test PUBLIC /EHsc /GR-)
    else()
        if(SKITY_CPP_ALLOCATION_RECOVERY)
            target_compile_options(skity_vulkan_unit_test PUBLIC -frtti)
        else()
            target_compile_options(skity_vulkan_unit_test PUBLIC -fno-rtti)
        endif()
    endif()
    if (${SKITY_TEST_COVERAGE} AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(skity_vulkan_unit_test PRIVATE --coverage -O0 -g)
        target_link_options(skity_vulkan_unit_test PRIVATE --coverage)
    endif()
    if (SKITY_VK_TEST_RPATH)
        set_target_properties(skity_vulkan_unit_test PROPERTIES
            BUILD_RPATH "${SKITY_VK_TEST_RPATH}"
        )
    endif()

    gtest_add_tests(skity_vulkan_unit_test "" AUTO)
endif()



if(SKITY_CPP_ALLOCATION_RECOVERY)
  # Match core exception options in a distinct production-visibility image.
  # Internal unit-test symbol visibility must not hide consumer-boundary bugs.
  add_library(skity-recovery-exception-test-image SHARED
              base/recovery_exception_image.cc)
  set_target_properties(skity-recovery-exception-test-image PROPERTIES CXX_STANDARD 17)
  target_compile_options(skity-recovery-exception-test-image PRIVATE
                         $<TARGET_PROPERTY:skity,COMPILE_OPTIONS>)
  if(NOT MSVC)
    target_compile_options(skity-recovery-exception-test-image PRIVATE
                           -fvisibility=hidden -fvisibility-inlines-hidden)
  endif()
  # A distinct ordinary C++ consumer must contain no legacy -fno-rtti unit
  # objects: their local typeinfo can conceal the cross-image mismatch.
  add_executable(skity-recovery-exception-consumer base/recovery_exception_test.cc)
  set_target_properties(skity-recovery-exception-consumer PROPERTIES
                       CXX_STANDARD 17 OUTPUT_NAME skity_unit_test
                       RUNTIME_OUTPUT_DIRECTORY
                       "${CMAKE_BINARY_DIR}/recovery-exception-consumer/test/ut")
  target_link_libraries(skity-recovery-exception-consumer PRIVATE
                        skity-recovery-exception-test-image gtest_main)
  if(MSVC)
    target_compile_options(skity-recovery-exception-consumer PRIVATE /EHsc /GR)
  else()
    target_compile_options(skity-recovery-exception-consumer PRIVATE -fexceptions -frtti)
  endif()
  add_test(NAME skity-recovery-exception-consumer
           COMMAND skity-recovery-exception-consumer)
  # Real allocation refusal must reach shared-library code under sanitizers.
  # Private replacement operators bind locally in each tested image; only the
  # counter is shared. None of these support sources enter product builds.
  if(SKITY_BUILD_INSTALL)
    message(FATAL_ERROR "Recovery allocation tests are noninstalled; set SKITY_BUILD_INSTALL=OFF or disable unit tests")
  endif()
  add_library(skity-allocation-test-support SHARED base/allocation_failure.cc)
  target_compile_definitions(skity-allocation-test-support PRIVATE SKITY_ALLOCATION_TEST_SUPPORT_IMPL=1)
  set_target_properties(skity-allocation-test-support PROPERTIES CXX_STANDARD 17)
  add_library(skity-allocation-test-operators OBJECT base/allocation_operators.cc)
  set_target_properties(skity-allocation-test-operators PROPERTIES POSITION_INDEPENDENT_CODE ON CXX_STANDARD 17)
  target_include_directories(skity_unit_test PRIVATE ${CMAKE_CURRENT_LIST_DIR})
  foreach(recovery_image skity skity-codec skity_unit_test)
    if(TARGET ${recovery_image})
      target_sources(${recovery_image} PRIVATE $<TARGET_OBJECTS:skity-allocation-test-operators>)
      target_link_libraries(${recovery_image} PRIVATE skity-allocation-test-support)
    endif()
  endforeach()
  target_compile_definitions(skity_unit_test PRIVATE SKITY_CPP_ALLOCATION_RECOVERY=1)
  target_sources(skity_unit_test PRIVATE render/canvas_save_allocation_test.cc)
  if(CMAKE_SYSTEM_NAME STREQUAL "Darwin" AND SKITY_MTL_BACKEND)
    target_sources(skity_unit_test PRIVATE render/hw/canvas_layer_allocation_test.mm)
    set_source_files_properties(render/hw/canvas_layer_allocation_test.mm
      PROPERTIES COMPILE_FLAGS "-fobjc-arc")
    target_link_libraries(skity_unit_test PRIVATE "-framework Metal")
  endif()
  if(SKITY_IO_MODULE)
    target_compile_definitions(skity_unit_test PRIVATE SKITY_RECOVERY_TEST_IO=1)
  endif()
endif()

# The allocation operator object expression is not a source text file.
get_target_property(_skity_unit_sources skity_unit_test SOURCES)
list(FILTER _skity_unit_sources EXCLUDE REGEX "^[$]<")
gtest_add_tests(TARGET skity_unit_test SOURCES ${_skity_unit_sources})
