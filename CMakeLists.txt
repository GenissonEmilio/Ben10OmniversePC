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
    target_link_libraries(Ben10OmniverseRecomp PRIVATE ps3recomp_runtime)
    target_compile_options(Ben10OmniverseRecomp PRIVATE -fno-strict-aliasing -ffp-contract=off)
    set_target_properties(Ben10OmniverseRecomp PROPERTIES LINKER_LANGUAGE CXX)
    message(WARNING "Experimental PPU boot target. Ben 10 Omniverse SPU jobs and rendering still require validation.")
endif()
