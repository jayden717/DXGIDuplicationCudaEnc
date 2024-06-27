#pragma once

#include "NvEncoder/NvEncoderCuda.h"
#include "Defs.h"
#include "DDAImpl.h"
#include "Preproc.h"
#include "NvEncoder/NvEncoderD3D11.h"

class Core
{
    /// Demo Application Core class
#define returnIfError(x)\
    if (FAILED(x))\
    {\
        printf("%s: Line %d, File %s Returning error 0x%08x\n",__FUNCTION__, __LINE__, __FILE__, x);\
        return x;\
    }

protected:

    /// PreProcessor for encoding. Defined in Preproc.h
    /// Preprocessing is required if captured images are of different size than encWidthxencHeight
    /// This application always uses this preprocessor
    //RGBToNV12 *pColorConv = nullptr;
    /// NVENCODE API wrapper. Defined in NvEncoderD3D11.h. This class is imported from NVIDIA Video SDK
    // NvEncoderD3D11 *pEnc = nullptr;

    /// D3D11 YUV420 Texture2D object that sends the image to NVENC for video encoding
    ID3D11Texture2D *pEncBuf = nullptr;
    /// Output video bitstream file handle
    FILE *fp = nullptr;
    /// Video output dimensions
    const static UINT encWidth = 1920;
    const static UINT encHeight = 1080;
    /// turn off preproc, let NVENCODE API handle colorspace conversion
    const static bool bNoVPBlt = true;
    /// Video output file name
    const char fnameBase[64] = "DDATest_%d.bgra";
    /// Encoded video bitstream packet in CPU memory
    std::vector<std::vector<uint8_t>> vPacket;
    /// NVENCODEAPI session intialization parameters
    NV_ENC_INITIALIZE_PARAMS encInitParams = { 0 };
    /// NVENCODEAPI video encoding configuration parameters
    NV_ENC_CONFIG encConfig = { 0 };


protected:



    /// Initialize NVENCODEAPI wrapper
    virtual HRESULT InitEnc() = 0;
    // {
    //     if (!pEnc)
    //     {
    //         DWORD w = bNoVPBlt ? pDDAWrapper->getWidth() : encWidth;
    //         DWORD h = bNoVPBlt ? pDDAWrapper->getHeight() : encHeight;
    //         NV_ENC_BUFFER_FORMAT fmt = bNoVPBlt ? NV_ENC_BUFFER_FORMAT_ARGB : NV_ENC_BUFFER_FORMAT_NV12;
    //         pEnc = new NvEncoderD3D11(pD3DDev, w, h, fmt);
    //         if (!pEnc)
    //         {
    //             returnIfError(E_FAIL);
    //         }

    //         ZeroMemory(&encInitParams, sizeof(encInitParams));
    //         ZeroMemory(&encConfig, sizeof(encConfig));
    //         encInitParams.encodeConfig = &encConfig;
    //         encInitParams.encodeWidth = w;
    //         encInitParams.encodeHeight = h;
    //         encInitParams.maxEncodeWidth = pDDAWrapper->getWidth();
    //         encInitParams.maxEncodeHeight = pDDAWrapper->getHeight();
    //         encConfig.gopLength = 5;

    //         try
    //         {
    //             pEnc->CreateDefaultEncoderParams(&encInitParams, NV_ENC_CODEC_H264_GUID, NV_ENC_PRESET_LOW_LATENCY_HP_GUID);
    //             pEnc->CreateEncoder(&encInitParams);
    //         }
    //         catch (...)
    //         {
    //             returnIfError(E_FAIL);
    //         }
    //     }
    //     return S_OK;
    // }

    /// Initialize preprocessor
//    HRESULT InitColorConv()
//    {
//        if (!pColorConv)
//        {
//            pColorConv = new RGBToNV12(pD3DDev, pCtx);
//            HRESULT hr = pColorConv->Init();
//            returnIfError(hr);
//        }
//        return S_OK;
//    }

    /// Initialize Video output file
//    HRESULT InitOutFile()
//    {
//        if (!fp)
//        {
//            char fname[64] = { 0 };
//            sprintf_s(fname, (const char *)fnameBase, failCount);
//            errno_t err = fopen_s(&fp, fname, "wb");
//            returnIfError(err);
//        }
//        return S_OK;
//    }



public:
    /// Initialize demo application
//    HRESULT Init()
//    {


//        hr = InitEnc();
//        returnIfError(hr);

//        hr = InitColorConv();
//        returnIfError(hr);

//        hr = InitOutFile();
//        returnIfError(hr);
//        return hr;
//    }




    /// Encode the captured frame using NVENCODEAPI
    virtual HRESULT Encode() = 0;
    // {
    //     HRESULT hr = S_OK;
    //     try
    //     {
    //         pEnc->EncodeFrame(vPacket);
    //         WriteEncOutput();
    //     }
    //     catch (...)
    //     {
    //         hr = E_FAIL;
    //     }
    // SAFE_RELEASE(pEncBuf);
    //     return hr;
    // }
    /// Release all resources
    void Cleanup(bool bDelete = true);
    Core(){}
    ~Core()
    {
        Cleanup(true);
    }
};
