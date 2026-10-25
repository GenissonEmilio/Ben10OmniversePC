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
option(PS3RECOMP_BUILD_TESTS "Build runtime regressions" ON)
option(PS3RECOMP_RSX_VULKAN "Build the Linux Vulkan backend" ON)
option(PS3RECOMP_FFMPEG_RUNTIME "Decode media with runtime FFmpeg" ON)
include(CTest)
add_subdirectory("${PS3RECOMP_DIR}" ps3recomp)

if(BUILD_TESTING AND UNIX)
    add_executable(test_ben10_flat_vm tests/test_flat_vm.c)
    target_include_directories(test_ben10_flat_vm PRIVATE host "${PS3RECOMP_DIR}/runtime/memory")
    add_test(NAME ben10_flat_vm COMMAND test_ben10_flat_vm)
endif()
if(BUILD_TESTING AND CMAKE_SYSTEM_NAME STREQUAL "Linux")
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
    add_custom_command(OUTPUT "${HLE_NIDS}"
        COMMAND "${Python3_EXECUTABLE}" "${PS3RECOMP_DIR}/tools/gen_hle_nids.py" --out "${HLE_NIDS}" --all
        DEPENDS "${PS3RECOMP_DIR}/tools/gen_hle_nids.py" "${PS3RECOMP_DIR}/tools/nid_database.py"
        VERBATIM)
    add_executable(Ben10OmniverseRecomp
        "${CMAKE_CURRENT_SOURCE_DIR}/host/boot_linux.cpp"
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
    target_compile_options(Ben10OmniverseRecomp PRIVATE -fno-strict-aliasing -ffp-contract=off)
    set_target_properties(Ben10OmniverseRecomp PROPERTIES LINKER_LANGUAGE CXX)
    message(WARNING "Experimental PPU boot target. Ben 10 Omniverse SPU jobs and rendering still require validation.")
endif()
