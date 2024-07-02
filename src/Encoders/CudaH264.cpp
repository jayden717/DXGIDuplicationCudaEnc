#include "CudaH264.hpp"
#include "cudad3d11.h"

CudaH264::CudaH264(int _argc, char *_argv[])
try : argc(_argc), argv(_argv), fpOut("out.bgra", std::ios::out | std::ios::binary), iGpu(0)
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
    std::cerr << "Failed to initialize CudaH264." << std::endl;
    throw;
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
HRESULT CudaH264::SaveFrameToFile(const void *pBuffer, int width, int height)
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
    fpOut.write(reinterpret_cast<const char *>(pBuffer), frameSize);

    return S_OK;
}

HRESULT CudaH264::InitOutFile()
{
    if (!fpOut)
    {
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
    if (cuContext == NULL)
    {
        std::ostringstream err;
        std::cerr << res << std::endl;
        err << "Unable to create CUDA context" << std::endl;
        throw std::invalid_argument(err.str());
    }
    NV_ENC_BUFFER_FORMAT eFormat = NV_ENC_BUFFER_FORMAT_ABGR;
    int iGpu = 0;
    try
    {
        pEnc = new NvEncoderCuda(cuContext, w, h, eFormat); // TODO Error management
    }
    catch (std::exception &error)
    {
        std::cerr << error.what() << std::endl;
    }

    NV_ENC_INITIALIZE_PARAMS initializeParams = {NV_ENC_INITIALIZE_PARAMS_VER};
    NV_ENC_CONFIG encodeConfig = {NV_ENC_CONFIG_VER};
    ZeroMemory(&initializeParams, sizeof(initializeParams));
    ZeroMemory(&encodeConfig, sizeof(encodeConfig));
    initializeParams.encodeConfig = &encodeConfig;
    initializeParams.encodeWidth = w;
    initializeParams.encodeHeight = h;
    pEnc->CreateDefaultEncoderParams(&initializeParams, NV_ENC_CODEC_H264_GUID, NV_ENC_PRESET_LOW_LATENCY_HP_GUID);

    pEnc->CreateEncoder(&initializeParams);
    return hr;
}

HRESULT CudaH264::Encode()
{
    HRESULT hr = S_OK;
    const NvEncInputFrame *encoderInputFrame = pEnc->GetNextInputFrame();
    NvEncoderCuda::CopyToDeviceFrame(cuContext,
                                     reinterpret_cast<void *>(dptr), // Use the CUDA device pointer directly
                                     0,
                                     (CUdeviceptr)encoderInputFrame->inputPtr,
                                     encoderInputFrame->pitch,
                                     pEnc->GetEncodeWidth(),
                                     pEnc->GetEncodeHeight(),
                                     CU_MEMORYTYPE_DEVICE, // Data is now on device
                                     encoderInputFrame->bufferFormat,
                                     encoderInputFrame->chromaOffsets,
                                     encoderInputFrame->numChromaPlanes);
    try
    {
        pEnc->EncodeFrame(vPacket);
        WriteEncOutput();
    }
    catch (...)
    {
        hr = E_FAIL;
    }
    return hr;
}

void CudaH264::Cleanup(bool bDelete)
{
    if (pDDAWrapper)
    {
        pDDAWrapper->Cleanup();
        delete pDDAWrapper;
        pDDAWrapper = nullptr;
    }
    SAFE_RELEASE(pDupTex2D);
    if (bDelete)
    {
        if (pEnc)
        {
            /// Flush the encoder and write all output to file before destroying the encoder
            pEnc->EndEncode(vPacket);
            WriteEncOutput();
            pEnc->DestroyEncoder();
            ZeroMemory(&initializeParams, sizeof(NV_ENC_INITIALIZE_PARAMS));
            ZeroMemory(&encodeConfig, sizeof(NV_ENC_CONFIG));
        }

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
    HRESULT hr = pDDAWrapper->GetCapturedFrame(&pDupTex2D, wait);
    pEncBuf = nullptr;
    if (FAILED(hr))
    {
        failCount++;
    }
    // Define the desired texture description
    D3D11_TEXTURE2D_DESC desc;
    ZeroMemory(&desc, sizeof(desc));
    desc.Width = 1920;
    desc.Height = 1080;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = 40;
    desc.CPUAccessFlags = 0;
    desc.MiscFlags = D3D11_RESOURCE_MISC_SHARED;

    // Create the intermediate texture
    hr = pD3DDev->CreateTexture2D(&desc, nullptr, &pEncBuf);


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
        fpOut.write(reinterpret_cast<char *>(packet.data()), packet.size());
    }
}

HRESULT CudaH264::Preproc()
{
    HRESULT hr = S_OK;
    size_t size;
    CUgraphicsResource cuResource;

    // declare texDesc;
    D3D11_TEXTURE2D_DESC texDesc;
    pDupTex2D->GetDesc(&texDesc);
    // verify if the texture is valid
    if (!pDupTex2D)
    {
        std::cerr << "Invalid texture." << std::endl;
        return E_FAIL;
    }
    // print the desc
    // Print the texture description
    std::cout << "Texture Description:" << std::endl;
    std::cout << "  Width: " << texDesc.Width << std::endl;
    std::cout << "  Height: " << texDesc.Height << std::endl;
    std::cout << "  MipLevels: " << texDesc.MipLevels << std::endl;
    std::cout << "  ArraySize: " << texDesc.ArraySize << std::endl;
    std::cout << "  Format: " << texDesc.Format << std::endl;
    std::cout << "  SampleDesc.Count: " << texDesc.SampleDesc.Count << std::endl;
    std::cout << "  SampleDesc.Quality: " << texDesc.SampleDesc.Quality << std::endl;
    std::cout << "  Usage: " << texDesc.Usage << std::endl;
    std::cout << "  BindFlags: " << texDesc.BindFlags << std::endl;
    std::cout << "  CPUAccessFlags: " << texDesc.CPUAccessFlags << std::endl;
    std::cout << "  MiscFlags: " << texDesc.MiscFlags << std::endl;

    pEncBuf->GetDesc(&texDesc);
        std::cout << "Copy Texture Description:" << std::endl;
    std::cout << "  Width: " << texDesc.Width << std::endl;
    std::cout << "  Height: " << texDesc.Height << std::endl;
    std::cout << "  MipLevels: " << texDesc.MipLevels << std::endl;
    std::cout << "  ArraySize: " << texDesc.ArraySize << std::endl;
    std::cout << "  Format: " << texDesc.Format << std::endl;
    std::cout << "  SampleDesc.Count: " << texDesc.SampleDesc.Count << std::endl;
    std::cout << "  SampleDesc.Quality: " << texDesc.SampleDesc.Quality << std::endl;
    std::cout << "  Usage: " << texDesc.Usage << std::endl;
    std::cout << "  BindFlags: " << texDesc.BindFlags << std::endl;
    std::cout << "  CPUAccessFlags: " << texDesc.CPUAccessFlags << std::endl;
    std::cout << "  MiscFlags: " << texDesc.MiscFlags << std::endl;

    // Register the D3D11 resource with CUDA with context

    CUresult cudaStatus = cuGraphicsD3D11RegisterResource(&cuResource, pEncBuf, CU_GRAPHICS_REGISTER_FLAGS_NONE);
    if (cudaStatus != CUDA_SUCCESS)
    {
        std::cerr << "Failed to register D3D11 resource with CUDA. : cudaError : " << cudaStatus << std::endl;
        return E_FAIL;
    }
    cudaStatus = cuGraphicsMapResources(1, &cuResource, 0);
    if (cudaStatus != CUDA_SUCCESS)
    {
        std::cerr << "Failed to map D3D11 resource to CUDA." << std::endl;
        return E_FAIL;
    }
    CUdeviceptr dptr;
    size_t size;

    cudaStatus = cuGraphicsResourceGetMappedPointer(&dptr, &size, cuResource);
    // Ensure the CUDA resource is mapped correctly
    if (cudaStatus != CUDA_SUCCESS)
        std::cerr << "Failed to get CUDA device pointer. : cudaError : " << cudaStatus << std::endl;
    if (!dptr)
    {
        std::cerr << "Invalid device pointer." << std::endl;
        cuGraphicsUnmapResources(1, &cuResource, 0);
        return E_FAIL;
    }
    Encode();
    returnIfError(hr);
    return hr;
}