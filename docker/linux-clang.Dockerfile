FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    bash \
    build-essential \
    ca-certificates \
    clang-18 \
    cmake \
    git \
    glslang-tools \
    libc++-18-dev \
    libc++abi-18-dev \
    libgl1-mesa-dev \
    libunwind-18-dev \
    libvulkan-dev \
    libwayland-dev \
    libxcursor-dev \
    libxi-dev \
    libxinerama-dev \
    libxkbcommon-dev \
    libxrandr-dev \
    lld-18 \
    ninja-build \
    pkg-config \
    python3 \
    vulkan-utility-libraries-dev \
    vulkan-validationlayers \
    wayland-protocols \
    && rm -rf /var/lib/apt/lists/*

ENV CC=clang-18
ENV CXX=clang++-18

WORKDIR /workspace
CMD ["bash"]
