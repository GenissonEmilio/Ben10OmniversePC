FROM ubuntu:24.04
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
    ca-certificates clang-18 lld-18 cmake ninja-build ccache pkg-config \
    libsdl2-dev libvulkan-dev glslang-dev glslang-tools spirv-cross \
    libspirv-cross-c-shared-dev libavcodec-dev libavformat-dev libavutil-dev \
    libswresample-dev libswscale-dev python3 python3-pip python3-venv \
    p7zip-full git && rm -rf /var/lib/apt/lists/*
RUN apt-get update && apt-get install -y --no-install-recommends \
    libclang-rt-18-dev && rm -rf /var/lib/apt/lists/*
ENV CC=clang-18 CXX=clang++-18 CMAKE_BUILD_PARALLEL_LEVEL=2
WORKDIR /workspace
