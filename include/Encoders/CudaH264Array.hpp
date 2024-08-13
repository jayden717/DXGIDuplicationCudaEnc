#pragma once

#include "IEncoder.hpp"
#include <memory>
#include "DDAImpl.hpp"
#include "NvEnc.h"
#include "NvEncoderD3D11.h"
#include "D3D11TextureConverter.h"

class CudaH264Array : public IEncoder
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
    //NvEnc *pEnc;
    NvEncoderCuda *pEnc;
    //NvEncoderD3D11 *pEnc;
    /// D3D11 device context used for the operations demonstrated in this application
    ID3D11Device *pD3DDev = nullptr;
    CUdeviceptr dptr;
    size_t size;
    CUarray cuArray;


    /// D3D11 RGB Texture2D object that recieves the captured image from DDA
    ID3D11Texture2D *pDupTex2D = nullptr;

    /// D3D11 RGB Texture2D object that has the correct rights to send the image to NVENCCUDA for mapping and video encoding
    ID3D11Texture2D *m_pEncBuf = nullptr;
    /// m_pEncBuf Description


    /// D3D11 device context
    ID3D11DeviceContext *pCtx = nullptr;

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

    std::unique_ptr<D3D11TextureConverter> m_textureConverter;

    NV_ENC_BUFFER_FORMAT m_pixelFormat = NV_ENC_BUFFER_FORMAT_NV12;

public:
    explicit CudaH264Array(int argc, char *_argv[]);
    ~CudaH264Array() override;
    HRESULT InitEnc() override;
    HRESULT Encode() override;
    HRESULT Encode(CUarray cuArray);
    HRESULT WriteRawFrame(ID3D11Texture2D *pBuffer);
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

    void WriteEncOutput();

    HRESULT SaveFrameToFile(const void* pBuffer, int width, int height);
};
