cmake_minimum_required(VERSION 3.20)
project(Ben10OmniversePC VERSION 0.1.0 LANGUAGES C CXX)

# The per-title boot arrangement follows arnaldolima1/gow-ascension-pc
# (MIT). Addresses, patches and SPURS scheduling overrides from Ascension
# deliberately need independent evidence before they can be used for Ben 10 Omniverse.
set(CMAKE_C_STANDARD 17)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(PS3RECOMP_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/ps3recomp" CACHE PATH "Pinned runtime")
option(BEN10_BUILD_GAME "Build the game from a locally generated lift" OFF)
option(BEN10_SPU_AVX2 "Use AVX2 SPU helpers for this local machine" OFF)
option(PS3RECOMP_BUILD_TESTS "Build runtime regressions" ON)
option(PS3RECOMP_RSX_VULKAN "Build the Linux Vulkan backend" ON)
option(PS3RECOMP_FFMPEG_RUNTIME "Decode media with runtime FFmpeg" ON)
include(CTest)
add_subdirectory("${PS3RECOMP_DIR}" ps3recomp)

if(BUILD_TESTING AND UNIX)
    add_executable(test_ben10_job_pool tests/test_job_pool.cpp host/job_pool.cpp)
    target_include_directories(test_ben10_job_pool PRIVATE host)
    target_compile_options(test_ben10_job_pool PRIVATE -O1 -UNDEBUG)
    target_link_libraries(test_ben10_job_pool PRIVATE pthread)
    add_test(NAME ben10_job_pool COMMAND test_ben10_job_pool)
    add_test(NAME ben10_job_pool_disabled COMMAND test_ben10_job_pool disabled)
    add_test(NAME ben10_job_pool_four COMMAND test_ben10_job_pool four)
    set_tests_properties(ben10_job_pool ben10_job_pool_disabled ben10_job_pool_four PROPERTIES TIMEOUT 8)
    add_executable(test_ben10_window_offset tests/test_rsx_window_offset.c
        "${PS3RECOMP_DIR}/libs/video/rsx_dispatch.c")
    target_include_directories(test_ben10_window_offset PRIVATE "${PS3RECOMP_DIR}/include"
        "${PS3RECOMP_DIR}/libs/video")
    target_compile_options(test_ben10_window_offset PRIVATE -O1 -UNDEBUG)
    add_test(NAME ben10_window_offset COMMAND test_ben10_window_offset)
    add_executable(test_ben10_flat_vm tests/test_flat_vm.c)
    target_include_directories(test_ben10_flat_vm PRIVATE host "${PS3RECOMP_DIR}/runtime/memory")
    add_test(NAME ben10_flat_vm COMMAND test_ben10_flat_vm)
endif()
if(BUILD_TESTING AND CMAKE_SYSTEM_NAME STREQUAL "Linux")
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(BEN10_SDL2 REQUIRED IMPORTED_TARGET sdl2)
    add_executable(test_ben10_pad_file tests/test_pad_file.c
        host/keyboard_state.cpp
        "${PS3RECOMP_DIR}/libs/input/cellPad.c"
        "${PS3RECOMP_DIR}/runtime/platform/win32_compat.c")
    target_include_directories(test_ben10_pad_file PRIVATE "${PS3RECOMP_DIR}/include"
        "${PS3RECOMP_DIR}/libs/input" "${PS3RECOMP_DIR}/runtime/ppu" host)
    target_compile_options(test_ben10_pad_file PRIVATE -O1 -UNDEBUG -ffunction-sections -fdata-sections)
    target_link_options(test_ben10_pad_file PRIVATE -Wl,--gc-sections)
    target_link_libraries(test_ben10_pad_file PRIVATE PkgConfig::BEN10_SDL2 pthread m)
    add_test(NAME ben10_pad_file COMMAND test_ben10_pad_file)
    set_tests_properties(ben10_pad_file PROPERTIES TIMEOUT 8)
    add_executable(test_ben10_taskset_argument tests/test_taskset_argument.c
        "${PS3RECOMP_DIR}/libs/spurs/spurs_taskset.c")
    target_include_directories(test_ben10_taskset_argument PRIVATE "${PS3RECOMP_DIR}/include"
        "${PS3RECOMP_DIR}/runtime/spu" "${PS3RECOMP_DIR}/runtime/platform")
    target_compile_options(test_ben10_taskset_argument PRIVATE -O1 -UNDEBUG -ffunction-sections -fdata-sections)
    target_link_options(test_ben10_taskset_argument PRIVATE -Wl,--gc-sections)
    target_link_libraries(test_ben10_taskset_argument PRIVATE pthread m)
    add_test(NAME ben10_taskset_argument COMMAND test_ben10_taskset_argument)
    add_executable(test_ben10_initial_stack tests/test_ppu_initial_stack.c)
    target_include_directories(test_ben10_initial_stack PRIVATE "${PS3RECOMP_DIR}/runtime/ppu")
    target_compile_options(test_ben10_initial_stack PRIVATE -O1 -UNDEBUG)
    add_test(NAME ben10_initial_stack COMMAND test_ben10_initial_stack)
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/prx-test")
    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/prx-test/ppu_recomp.h" "#include \"ppu_context.h\"\n")
    add_executable(test_ben10_prx_stack tests/test_prx_stack_arguments.cpp
        "${PS3RECOMP_DIR}/runtime/ppu/ppu_hle.cpp")
    target_include_directories(test_ben10_prx_stack PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/prx-test"
        "${PS3RECOMP_DIR}/include" "${PS3RECOMP_DIR}/runtime/ppu")
    target_compile_options(test_ben10_prx_stack PRIVATE -O1 -UNDEBUG -ffunction-sections -fdata-sections)
    target_link_options(test_ben10_prx_stack PRIVATE -Wl,--gc-sections)
    add_test(NAME ben10_prx_stack COMMAND test_ben10_prx_stack)
    add_executable(test_ben10_spurs_queue tests/test_spurs_queue.c
        "${PS3RECOMP_DIR}/runtime/platform/win32_compat.c")
    target_include_directories(test_ben10_spurs_queue PRIVATE "${PS3RECOMP_DIR}/include"
        "${PS3RECOMP_DIR}/runtime/spu" "${PS3RECOMP_DIR}/runtime/platform")
    target_compile_options(test_ben10_spurs_queue PRIVATE -O1 -UNDEBUG -ffunction-sections -fdata-sections)
    target_link_options(test_ben10_spurs_queue PRIVATE -Wl,--gc-sections)
    target_link_libraries(test_ben10_spurs_queue PRIVATE pthread m)
    add_test(NAME ben10_spurs_queue COMMAND test_ben10_spurs_queue)
    set_tests_properties(ben10_spurs_queue PROPERTIES TIMEOUT 8)
    add_executable(test_ben10_guest_strchr tests/test_guest_strchr.c)
    target_include_directories(test_ben10_guest_strchr PRIVATE "${PS3RECOMP_DIR}/include"
        "${PS3RECOMP_DIR}/runtime/platform")
    target_compile_options(test_ben10_guest_strchr PRIVATE -O1 -UNDEBUG -ffunction-sections -fdata-sections)
    target_link_options(test_ben10_guest_strchr PRIVATE -Wl,--gc-sections)
    add_test(NAME ben10_guest_strchr COMMAND test_ben10_guest_strchr)
    add_executable(test_ben10_savedata_user_io tests/test_savedata_user_io.c)
    target_include_directories(test_ben10_savedata_user_io PRIVATE "${PS3RECOMP_DIR}/include")
    target_compile_options(test_ben10_savedata_user_io PRIVATE -O1 -UNDEBUG -ffunction-sections -fdata-sections)
    target_link_options(test_ben10_savedata_user_io PRIVATE -Wl,--gc-sections)
    add_test(NAME ben10_savedata_user_io COMMAND test_ben10_savedata_user_io)
    add_executable(test_ben10_overlay_residence tests/test_spu_overlay_residence.c)
    target_include_directories(test_ben10_overlay_residence PRIVATE "${PS3RECOMP_DIR}/include"
        "${PS3RECOMP_DIR}/runtime/spu" "${PS3RECOMP_DIR}/runtime/platform")
    target_compile_options(test_ben10_overlay_residence PRIVATE -O1 -UNDEBUG -ffunction-sections -fdata-sections)
    target_link_options(test_ben10_overlay_residence PRIVATE -Wl,--gc-sections)
    target_link_libraries(test_ben10_overlay_residence PRIVATE pthread m)
    add_test(NAME ben10_overlay_residence COMMAND test_ben10_overlay_residence)
    add_executable(test_ben10_job_ring tests/test_job_ring.c
        "${PS3RECOMP_DIR}/runtime/platform/win32_compat.c")
    target_include_directories(test_ben10_job_ring PRIVATE "${PS3RECOMP_DIR}/include"
        "${PS3RECOMP_DIR}/runtime/spu" "${PS3RECOMP_DIR}/runtime/platform")
    target_compile_options(test_ben10_job_ring PRIVATE -O1 -UNDEBUG -ffunction-sections -fdata-sections)
    target_link_options(test_ben10_job_ring PRIVATE -Wl,--gc-sections)
    target_link_libraries(test_ben10_job_ring PRIVATE pthread m)
    add_test(NAME ben10_job_ring COMMAND test_ben10_job_ring)
    set_tests_properties(ben10_job_ring PROPERTIES TIMEOUT 8)
endif()

if(BEN10_BUILD_GAME)
    set(RECOMP_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/recomp" CACHE PATH "Local Ben 10 Omniverse PPU lift")
    if(NOT EXISTS "${RECOMP_DIR}/ppu_recomp.h")
        message(FATAL_ERROR "Ben 10 Omniverse has not been lifted. Analyze your own ELF first; see README.md.")
    endif()
    file(GLOB RECOMP_SOURCES CONFIGURE_DEPENDS "${RECOMP_DIR}/*.c" "${RECOMP_DIR}/*.cpp")
    if(NOT RECOMP_SOURCES)
        message(FATAL_ERROR "No recompiled Ben 10 Omniverse functions in ${RECOMP_DIR}")
    endif()
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    set(HLE_NIDS "${CMAKE_CURRENT_BINARY_DIR}/ppu_hle_nids.cpp")
    file(GLOB_RECURSE HLE_LIBRARY_INPUTS CONFIGURE_DEPENDS "${PS3RECOMP_DIR}/libs/*.c")
    add_custom_command(OUTPUT "${HLE_NIDS}"
        COMMAND "${Python3_EXECUTABLE}" "${PS3RECOMP_DIR}/tools/gen_hle_nids.py" --out "${HLE_NIDS}" --all
        DEPENDS "${PS3RECOMP_DIR}/tools/gen_hle_nids.py" "${PS3RECOMP_DIR}/tools/nid_database.py" ${HLE_LIBRARY_INPUTS}
        VERBATIM)
    add_executable(Ben10OmniverseRecomp
        "${CMAKE_CURRENT_SOURCE_DIR}/host/boot_linux.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/host/job_pool.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/host/keyboard_state.cpp"
        "${PS3RECOMP_DIR}/runtime/ppu/ppu_loader.cpp"
        "${PS3RECOMP_DIR}/runtime/ppu/ppu_hle.cpp"
        "${PS3RECOMP_DIR}/runtime/ppu/ppu_fs.cpp"
        "${PS3RECOMP_DIR}/runtime/ppu/ppu_sysprx.cpp"
        "${HLE_NIDS}" ${RECOMP_SOURCES})
    target_include_directories(Ben10OmniverseRecomp PRIVATE
        "${RECOMP_DIR}" "${PS3RECOMP_DIR}/include"
        "${PS3RECOMP_DIR}/runtime/ppu" "${PS3RECOMP_DIR}/runtime/spu"
        "${PS3RECOMP_DIR}/runtime/memory" "${PS3RECOMP_DIR}/runtime/syscalls"
        "${PS3RECOMP_DIR}/runtime/platform" "${PS3RECOMP_DIR}/libs/video")
    set_source_files_properties(${RECOMP_SOURCES} PROPERTIES COMPILE_OPTIONS "-O1;-w;-ffp-contract=off")
    set(SPU_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/spu" CACHE PATH "Locally translated SPU workloads")
    file(GLOB SPU_SOURCES CONFIGURE_DEPENDS "${SPU_DIR}/*/spu_recomp.c")
    if(SPU_SOURCES AND EXISTS "${SPU_DIR}/spu_workloads.c")
        list(APPEND SPU_SOURCES "${SPU_DIR}/spu_workloads.c"
             "${PS3RECOMP_DIR}/runtime/spu/spu_dispatch_mt.c")
        target_sources(Ben10OmniverseRecomp PRIVATE ${SPU_SOURCES})
        set_source_files_properties(${SPU_SOURCES} PROPERTIES
            COMPILE_OPTIONS "-O1;-w;-ffp-contract=off"
            COMPILE_DEFINITIONS "SPU_USE_DISPATCH_MT")
    endif()
    set(SPU_JOB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/spu_jobs" CACHE PATH "Locally translated raw SPURS jobs")
    file(GLOB JOB_SOURCES CONFIGURE_DEPENDS "${SPU_JOB_DIR}/*/spu_recomp.c")
    if(JOB_SOURCES AND EXISTS "${SPU_JOB_DIR}/spu_jobs.c")
        list(APPEND JOB_SOURCES "${SPU_JOB_DIR}/spu_jobs.c")
        target_sources(Ben10OmniverseRecomp PRIVATE ${JOB_SOURCES})
        set_source_files_properties(${JOB_SOURCES} PROPERTIES
            COMPILE_OPTIONS "-O1;-w;-ffp-contract=off"
            COMPILE_DEFINITIONS "SPU_USE_DISPATCH_MT")
    endif()
    set(SPU_OVERLAY_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/spu_overlays" CACHE PATH "Locally translated dynamic SPU plugins")
    file(GLOB OVERLAY_SOURCES CONFIGURE_DEPENDS "${SPU_OVERLAY_DIR}/*/spu_recomp.c")
    if(OVERLAY_SOURCES AND EXISTS "${SPU_OVERLAY_DIR}/spu_overlays.c")
        list(APPEND OVERLAY_SOURCES "${SPU_OVERLAY_DIR}/spu_overlays.c")
        target_sources(Ben10OmniverseRecomp PRIVATE ${OVERLAY_SOURCES})
        set_source_files_properties(${OVERLAY_SOURCES} PROPERTIES
            COMPILE_OPTIONS "-O1;-w;-ffp-contract=off"
            COMPILE_DEFINITIONS "SPU_USE_DISPATCH_MT")
    endif()
    target_link_libraries(Ben10OmniverseRecomp PRIVATE ps3recomp_runtime)
    set(SPU_CODEC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/spu_codecs" CACHE PATH "Local media codec SPU lifts")
    file(GLOB CODEC_SOURCES CONFIGURE_DEPENDS "${SPU_CODEC_DIR}/*/spu_recomp.c")
    if(CODEC_SOURCES AND EXISTS "${SPU_CODEC_DIR}/spu_codecs.c")
        list(APPEND CODEC_SOURCES "${SPU_CODEC_DIR}/spu_codecs.c")
        if(EXISTS "${SPU_CODEC_DIR}/spu_codec_variants.c")
            list(APPEND CODEC_SOURCES "${SPU_CODEC_DIR}/spu_codec_variants.c")
        endif()
        target_sources(Ben10OmniverseRecomp PRIVATE ${CODEC_SOURCES})
        set_source_files_properties(${CODEC_SOURCES} PROPERTIES
            COMPILE_OPTIONS "-O1;-w;-ffp-contract=off"
            COMPILE_DEFINITIONS "SPU_USE_DISPATCH_MT")
    endif()
    set(SPU_FIRMWARE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/spu_firmware" CACHE PATH "Locally translated firmware SPU codecs")
    file(GLOB FIRMWARE_SOURCES CONFIGURE_DEPENDS "${SPU_FIRMWARE_DIR}/*/spu_recomp.c")
    if(FIRMWARE_SOURCES AND EXISTS "${SPU_FIRMWARE_DIR}/spu_firmware.c")
        list(APPEND FIRMWARE_SOURCES "${SPU_FIRMWARE_DIR}/spu_firmware.c")
        target_sources(Ben10OmniverseRecomp PRIVATE ${FIRMWARE_SOURCES})
        set_source_files_properties(${FIRMWARE_SOURCES} PROPERTIES
            COMPILE_OPTIONS "-O1;-w;-ffp-contract=off"
            COMPILE_DEFINITIONS "SPU_USE_DISPATCH_MT")
    endif()
    target_compile_options(Ben10OmniverseRecomp PRIVATE -fno-strict-aliasing -ffp-contract=off)
    set(FIRMWARE_PPU_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/firmware_ppu" CACHE PATH "Local optional media firmware lift")
    file(GLOB FIRMWARE_PPU_SOURCES CONFIGURE_DEPENDS "${FIRMWARE_PPU_DIR}/*/lifted/*.cpp")
    if(FIRMWARE_PPU_SOURCES AND EXISTS "${FIRMWARE_PPU_DIR}/modules.cpp")
        list(APPEND FIRMWARE_PPU_SOURCES "${FIRMWARE_PPU_DIR}/modules.cpp")
        target_sources(Ben10OmniverseRecomp PRIVATE ${FIRMWARE_PPU_SOURCES})
        set_source_files_properties(${FIRMWARE_PPU_SOURCES} PROPERTIES
            COMPILE_OPTIONS "-O1;-w;-ffp-contract=off"
            INCLUDE_DIRECTORIES "${PS3RECOMP_DIR}/runtime/prx")
    endif()
    set_target_properties(Ben10OmniverseRecomp PROPERTIES LINKER_LANGUAGE CXX)
    set(BEN10_ALL_SPU_SOURCES ${SPU_SOURCES} ${JOB_SOURCES} ${OVERLAY_SOURCES} ${FIRMWARE_SOURCES} ${CODEC_SOURCES})
    if(BEN10_ALL_SPU_SOURCES)
        set_property(SOURCE ${BEN10_ALL_SPU_SOURCES} APPEND PROPERTY COMPILE_OPTIONS -O3)
        if(BEN10_SPU_AVX2)
            set_property(SOURCE ${BEN10_ALL_SPU_SOURCES} APPEND PROPERTY COMPILE_OPTIONS -mavx2)
        endif()
    endif()
    message(WARNING "Experimental PPU boot target. Ben 10 Omniverse SPU jobs and rendering still require validation.")
endif()
