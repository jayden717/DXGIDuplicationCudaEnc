#pragma once

#include "IEncoder.hpp"
#include <memory>
#include "DDAImpl.h"
#include "NvEncoder/NvEncoderCuda.h"
//#include "Utils/NvEncoderCLIOptions.h"
#include "NvEncoder/NvEncoderD3D11.h"
//#include "Utils/Logger.h"

class CudaH264 : public IEncoder
{
    #define returnIfError(x)\
    if (FAILED(x))\
    {\
        printf("%s: Line %d, File %s Returning error 0x%08x\n",__FUNCTION__, __LINE__, __FILE__, x);\
        return x;\
    }
private:
    CUdevice cuDevice = 0;
    /// Cuda device context used for the operations demonstrated in this application
    CUcontext cuContext;

    /// DDA wrapper object, defined in DDAImpl.h
    DDAImpl *pDDAWrapper = nullptr;


    /// NVENCODE API wrapper. Defined in NvEncoderCuda.h. This class is imported from NVIDIA Video SDK
    NvEncoderCuda *pEnc;
    /// D3D11 device context used for the operations demonstrated in this application
    ID3D11Device *pD3DDev = nullptr;
    CUdeviceptr dptr;


    /// D3D11 RGB Texture2D object that recieves the captured image from DDA
    ID3D11Texture2D *pDupTex2D = nullptr;
//    /// D3D11 YUV420 Texture2D object that sends the image to NVENC for video encoding
//    ID3D11Texture2D *pEncBuf = nullptr;
    /// D3D11 device context
    ID3D11DeviceContext *pCtx = nullptr;
    ID3D11Texture2D *pEncBuf = nullptr;

    /// Encoded video bitstream packet in CPU memory
    std::vector<std::vector<uint8_t>> vPacket;

    /// Arguments for Cuda
    NV_ENC_INITIALIZE_PARAMS initializeParams = { NV_ENC_INITIALIZE_PARAMS_VER };
    NV_ENC_CONFIG encodeConfig = { NV_ENC_CONFIG_VER };
    //    NvEncoderInitParam encodeCLIOptions;
    int iGpu;
    std::ofstream fpOut;
    /// Failure count from Capture API
    UINT failCount = 0;
    char **argv;
    int argc;

public:
    explicit CudaH264(int argc, char *_argv[]);
    ~CudaH264() override;
    HRESULT InitEnc() override;
    HRESULT Encode() override;
    void Cleanup(bool bDelete) override;

    // Desktop duplication

    /// Just a method to init everything
    HRESULT Init();

    /// Initialize open the output file
    HRESULT InitOutFile();

     /// Initialize DDA handler
    HRESULT InitDup();

    /// Initialize DXGI pipeline
    HRESULT InitDXGI();

    /// Capture a frame using DDA
    HRESULT Capture(int wait);

    /// Preprocess captured frame
    HRESULT Preproc();

//    void ParseCommandLine(int argc, char *argv[], int &nWidth, int &nHeight,
//    NV_ENC_BUFFER_FORMAT &eFormat, char *szOutputFileName, NvEncoderInitParam &initParam, int &iGpu);
    void WriteEncOutput();

    HRESULT SaveFrameToFile(const void* pBuffer, int width, int height);
};


