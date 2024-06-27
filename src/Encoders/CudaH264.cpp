#include "CudaH264.hpp"

CudaH264::CudaH264(int _argc, char *_argv[])
try
: argc(_argc), argv(_argv), fpOut("out.bgra", std::ios::out | std::ios::binary), iGpu(0)
{

    int iGpu = 0;
    int nGpu = 0;
    cuDeviceGetCount(&nGpu);
    std::cout << nGpu << std::endl;
    cuInit(0);
    cuDeviceGet(&cuDevice, iGpu);
    char szDeviceName[80];
    cuDeviceGetName(szDeviceName, sizeof(szDeviceName), cuDevice);
    if (iGpu < 0 || iGpu >= nGpu)
    {
        std::cout << "GPU ordinal out of range. Should be within [" << 0 << ", " << nGpu - 1 << "]" << std::endl;
    }
    /// Parse arguments
//    CheckInputFile(szInFilePath);
//
//    if (!*szOutFilePath)
//        sprintf(szOutFilePath, encodeCLIOptions.IsCodecH264() ? "out.h264" : "out.hevc");

}
catch (...)
{
}

CudaH264::~CudaH264()
{
}
HRESULT CudaH264::Init()
{
    HRESULT hr = S_OK;
    hr = InitDXGI();
    returnIfError(hr);
    hr = InitDup();
    returnIfError(hr);
    hr = InitEnc();
    returnIfError(hr);
    hr = InitOutFile();
    returnIfError(hr);
    return hr;
}
HRESULT CudaH264::SaveFrameToFile(const void* pBuffer, int width, int height)
{
    if (!fpOut.is_open())
    {
        std::ostringstream err;
        err << "Output file is not open." << std::endl;
        throw std::runtime_error(err.str());
    }

    // Calculate frame size assuming RGBA or BGRA format (4 bytes per pixel)
    size_t frameSize = width * height * 4;

    // Write frame data to the output file
    fpOut.write(reinterpret_cast<const char*>(pBuffer), frameSize);

    return S_OK;
}

HRESULT CudaH264::InitOutFile()
{
    if (!fpOut) {
        std::ostringstream err;
        err << "Unable to open output file: out.bgra" << std::endl;
        throw std::invalid_argument(err.str());
    }
    return S_OK;
}
HRESULT CudaH264::InitEnc()
{
    HRESULT hr = S_OK;
    DWORD w = pDDAWrapper->getWidth();
    DWORD h = pDDAWrapper->getHeight();

    char szDeviceName[80];
    cuDeviceGet(&cuDevice, iGpu);
    cuDeviceGetName(szDeviceName, sizeof(szDeviceName), cuDevice);
    std::cout << "GPU in use: " << szDeviceName << std::endl;
    CUresult res = cuCtxCreate(&cuContext, 0, cuDevice);
    if (cuContext == NULL) {
        std::ostringstream err;
        std::cerr << res << std::endl;
        err << "Unable to create CUDA context" << std::endl;
        throw std::invalid_argument(err.str());
    }
    NV_ENC_BUFFER_FORMAT eFormat = NV_ENC_BUFFER_FORMAT_NV12;
    int iGpu = 0;
    try {
        pEnc = new NvEncoderCuda(cuContext, w, h, eFormat);// TODO Error management
    } catch (std::exception &error) {
        std::cerr << error.what() << std::endl;

    }

    NV_ENC_INITIALIZE_PARAMS initializeParams = { NV_ENC_INITIALIZE_PARAMS_VER };
    NV_ENC_CONFIG encodeConfig = { NV_ENC_CONFIG_VER };
    ZeroMemory(&initializeParams, sizeof(initializeParams));
    ZeroMemory(&encodeConfig, sizeof(encodeConfig));
    initializeParams.encodeConfig = &encodeConfig;
    initializeParams.encodeWidth = w;
    initializeParams.encodeHeight = h;
    pEnc->CreateDefaultEncoderParams(&initializeParams, NV_ENC_CODEC_H264_GUID, NV_ENC_PRESET_LOW_LATENCY_HP_GUID);

//    ParseCommandLine(argc, argv, w, h, eFormat, "out.h264", encodeCLIOptions, iGpu);
//    encodeCLIOptions.SetInitParams(&initializeParams, eFormat);

    pEnc->CreateEncoder(&initializeParams);
    return hr;

}

HRESULT CudaH264::Encode()
{
    HRESULT hr = S_OK;
    try
    {
        pEnc->EncodeFrame(vPacket);

//        WriteEncOutput();
    }
    catch (...)
    {
        hr = E_FAIL;
    }
    return hr;
}

void CudaH264::Cleanup(bool bDelete)
{
    if (pDDAWrapper) {
        pDDAWrapper->Cleanup();
        delete pDDAWrapper;
        pDDAWrapper = nullptr;
    }
    SAFE_RELEASE(pDupTex2D);
    if (bDelete)
    {
//        if (pEnc)
//        {
//            /// Flush the encoder and write all output to file before destroying the encoder
//            pEnc->EndEncode(vPacket);
//            WriteEncOutput();
//            pEnc->DestroyEncoder();
//            ZeroMemory(&encInitParams, sizeof(NV_ENC_INITIALIZE_PARAMS));
//            ZeroMemory(&encConfig, sizeof(NV_ENC_CONFIG));
//        }
//
//            if (pColorConv)
//            {
//                delete pColorConv;
//                pColorConv = nullptr;
//            }
            SAFE_RELEASE(pD3DDev);
            SAFE_RELEASE(pCtx);
        }
}

HRESULT CudaH264::InitDup()
{
    HRESULT hr = S_OK;
    if (!pDDAWrapper)
    {
        pDDAWrapper = new DDAImpl(pD3DDev, pCtx);
        hr = pDDAWrapper->Init();
        returnIfError(hr);
    }
    return hr;
}
HRESULT CudaH264::Capture(int wait)
{
        HRESULT hr = pDDAWrapper->GetCapturedFrame(&pDupTex2D, wait); // Release after preproc
        if (FAILED(hr))
        {
            failCount++;
        }
        return hr;
}

/// Initialize DXGI pipeline
HRESULT CudaH264::InitDXGI()
{
    HRESULT hr = S_OK;
    /// Driver types supported
    D3D_DRIVER_TYPE DriverTypes[] =
        {
            D3D_DRIVER_TYPE_HARDWARE,
            D3D_DRIVER_TYPE_WARP,
            D3D_DRIVER_TYPE_REFERENCE,
        };
    UINT NumDriverTypes = ARRAYSIZE(DriverTypes);

    /// Feature levels supported
    D3D_FEATURE_LEVEL FeatureLevels[] =
        {
            D3D_FEATURE_LEVEL_11_0,
            D3D_FEATURE_LEVEL_10_1,
            D3D_FEATURE_LEVEL_10_0,
            D3D_FEATURE_LEVEL_9_1};
    UINT NumFeatureLevels = ARRAYSIZE(FeatureLevels);
    D3D_FEATURE_LEVEL FeatureLevel = D3D_FEATURE_LEVEL_11_0;

    /// Create device
    for (UINT DriverTypeIndex = 0; DriverTypeIndex < NumDriverTypes; ++DriverTypeIndex)
    {
        hr = D3D11CreateDevice(nullptr, DriverTypes[DriverTypeIndex], nullptr, /*D3D11_CREATE_DEVICE_DEBUG*/ 0, FeatureLevels, NumFeatureLevels,
                               D3D11_SDK_VERSION, &pD3DDev, &FeatureLevel, &pCtx);
        if (SUCCEEDED(hr))
        {
            // Device creation succeeded, no need to loop anymore
            break;
        }
    }
    return hr;
}

/// Write encoded video output to file
void CudaH264::WriteEncOutput()
{
    int nFrame = 0;
    nFrame = (int)vPacket.size();
    for (std::vector<uint8_t> &packet : vPacket)
    {
        fpOut.write(reinterpret_cast<char*>(packet.data()), packet.size());
    }
}

HRESULT CudaH264::Preproc()
{
    HRESULT hr = S_OK;

//    std::unique_ptr<uint8_t[]> pHostFrame(new uint8_t[nFrameSize]);
//    Encode();
    if (SUCCEEDED(hr))
    {
        // Process captured frame if needed

        // Assuming pDupTex2D contains the captured frame data

        // Example: Get raw data from pDupTex2D (RGBA or BGRA format)
        D3D11_TEXTURE2D_DESC desc;
        pDupTex2D->GetDesc(&desc);
        const NvEncInputFrame *pEncInput = pEnc->GetNextInputFrame();
        pEncBuf = (ID3D11Texture2D *)pEncInput->inputPtr;

        pCtx->CopySubresourceRegion(pEncBuf, D3D11CalcSubresource(0, 0, 1), 0, 0, 0, pDupTex2D, 0, NULL);

        printf("Texture Description:\n");
        printf("  Width: %u\n", desc.Width);
        printf("  Height: %u\n", desc.Height);
        printf("  Mip Levels: %u\n", desc.MipLevels);
        printf("  Array Size: %u\n", desc.ArraySize);
        printf("  Format: %u\n", desc.Format);
        printf("  Usage: %u\n", desc.Usage);
        printf("  Bind Flags: %u\n", desc.BindFlags);
        printf("  CPU Access Flags: %u\n", desc.CPUAccessFlags);
        printf("  Misc Flags: %u\n", desc.MiscFlags);


//        hr = pCtx->Map(pCPUTexture2D, 0, D3D11_MAP_READ, 0, &mapped);
//        if (SUCCEEDED(hr))
//        {
            // Save frame data to file

            // Unmap the texture
//            pCtx->Unmap(pDupTex2D, 0);
//        }
    }


//    SAFE_RELEASE(pDupTex2D);
    printf("Failed to map texture, HRESULT: 0x%08X\n", hr);

    returnIfError(hr);

//    pEncBuf->AddRef();  // Release after encode
    return hr;
}