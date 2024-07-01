cmake_minimum_required(VERSION 3.28)
project(nvEncDXGIOutputDuplicationSample)

set(CMAKE_CXX_STANDARD 14)
set(CUDA_PATH "C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v12.5")
set(NVCODEC_PATH "${CMAKE_SOURCE_DIR}/NvCodec")
include_directories(
        ${CUDA_PATH}/include
        ${NVCODEC_PATH}
        ${NVCODEC_PATH}/NvDecoder
        ${NVCODEC_PATH}/NvEncoder
        include
        include/Encoders
)
# Paths

# Compiler and linker flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${CCFLAGS}")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${LDFLAGS}")
set(CUDA_LIBRARIES "${CUDA_PATH}/lib/x64/cuda.lib")

# Source files
set(SOURCES
        src/DDAImpl.cpp
        src/main.cpp
        ${NVCODEC_PATH}/NvEncoder/NvEncoder.cpp
        ${NVCODEC_PATH}/NvEncoder/NvEncoderCuda.cpp
        ${NVCODEC_PATH}/NvEncoder/NvEncoderD3D11.cpp
        include/DDAImpl.h
        include/Defs.h
        src/Encoders/CudaH264.cpp
        include/Encoders/CudaH264.hpp
        include/Encoders/IEncoder.hpp
)


add_executable(nvEncDXGIOutputDuplicationSample ${SOURCES})
# Link libraries
target_link_libraries(nvEncDXGIOutputDuplicationSample
        d3d11
        ${CUDA_LIBRARIES}
)
