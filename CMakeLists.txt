cmake_minimum_required(VERSION 3.25)
project(nvEncDXGIOutputDuplicationSample)

set(CMAKE_CXX_STANDARD 14)
#set(CUDA_PATH "C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v12.0")
set(NVCODEC_PATH "${CMAKE_SOURCE_DIR}/NvCodec")


set(CUDA_TOOLKIT_ROOT_DIR $ENV{CUDA_PATH})
message("${CUDA_TOOLKIT_ROOT_DIR}")

include_directories(
        ${CUDA_TOOLKIT_ROOT_DIR}/include
        ${NVCODEC_PATH}
        ${NVCODEC_PATH}/NvDecoder
        ${NVCODEC_PATH}/NvEncoder
        include
        include/Encoders
        Interface
        Utils
)


# Compiler and linker flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${CCFLAGS}")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${LDFLAGS}")
set(CUDA_LIBRARIES "${CUDA_TOOLKIT_ROOT_DIR}/lib/x64/cuda.lib" "${CUDA_TOOLKIT_ROOT_DIR}/lib/x64/cudart.lib" "${CMAKE_SOURCE_DIR}/Lib/x64/nvencodeapi.lib")
find_library(D3D_COMPILER_LIB d3dcompiler PATHS "C:/Program Files (x86)/Windows Kits/10/Lib/10.0.22621.0/um/x64")

# Source files
set(SOURCES
        src/DDAImpl.cpp
        src/main.cpp
        ${NVCODEC_PATH}/NvEncoder/NvEncoder.cpp
        ${NVCODEC_PATH}/NvEncoder/NvEncoderCuda.cpp
        ${NVCODEC_PATH}/NvEncoder/NvEncoderD3D11.cpp
        src/Encoders/CudaH264.cpp
        src/Encoders/CudaH264Array.cpp
        src/Encoders/D3D11TextureConverter.cpp
        src/Encoders/NvEnc.cpp
        include/Encoders/CudaH264.hpp
        include/Encoders/CudaH264Array.hpp
        include/Encoders/IEncoder.hpp
        include/Encoders/NvEnc.h
        include/Encoders/D3D11TextureConverter.h
)


add_executable(nvEncDXGIOutputDuplicationSample ${SOURCES})
# Link libraries
target_link_libraries(nvEncDXGIOutputDuplicationSample
        d3d11
        d3dcompiler
        ${CUDA_LIBRARIES}
)