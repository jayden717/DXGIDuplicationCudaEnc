#include "CudaH264.hpp"
#include "cudad3d11.h"
#include <wrl/client.h>
#include <iostream>
#include "d3dcompiler.h"
#include <fstream>
#include <winnt.h>

#include <cuda_runtime_api.h>

CudaH264::CudaH264(int _argc, char *_argv[])
try : argc(_argc), argv(_argv), fpOut("out.h264", std::ios::out | std::ios::binary), iGpu(0)
{
    int iGpu = 0;
    int nGpu = 0;
    std::cout << nGpu << std::endl;
    cuInit(0);
    cuDeviceGetCount(&nGpu);
    cuDeviceGet(&cuDevice, iGpu);
    char szDeviceName[80];
    cuDeviceGetName(szDeviceName, sizeof(szDeviceName), cuDevice);
    if (iGpu < 0 || iGpu >= nGpu)
    {
        std::cout << "GPU ordinal out of range. Should be within [" << 0 << ", " << nGpu - 1 << "]" << std::endl;
    }
}
catch (...)
{
    std::cerr << "Failed to initialize CudaH264." << std::endl;
    throw;
}

CudaH264::~CudaH264()
{
    Cleanup(true);
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

/// For raw writing
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
HRESULT CudaH264::WriteRawFrame(ID3D11Texture2D *pBuffer) // write pBuffer into a file .bgra file
{
    HRESULT hr = S_OK;
    D3D11_TEXTURE2D_DESC desc;
    pBuffer->GetDesc(&desc);
    D3D11_MAPPED_SUBRESOURCE mappedResource;
    hr = pCtx->Map(pBuffer, 0, D3D11_MAP_READ, 0, &mappedResource);
    if (FAILED(hr))
    {
        std::cerr << "Failed to map D3D11 texture for reading. Error code: " << std::hex << hr << std::endl;
        return hr;
    }

    // Write the mapped resource to the output file
    SaveFrameToFile(mappedResource.pData, desc.Width, desc.Height);

    pCtx->Unmap(pBuffer, 0);
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
            break;
    }
    return hr;
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
    NV_ENC_BUFFER_FORMAT eFormat = NV_ENC_BUFFER_FORMAT_ARGB;
    //NV_ENC_BUFFER_FORMAT eFormat = NV_ENC_BUFFER_FORMAT_NV12;
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
    NV_ENC_TUNING_INFO tuningInfo = NV_ENC_TUNING_INFO_ULTRA_LOW_LATENCY;
    pEnc->CreateDefaultEncoderParams(&initializeParams, NV_ENC_CODEC_H264_GUID, NV_ENC_PRESET_P3_GUID, tuningInfo);

    pEnc->CreateEncoder(&initializeParams);

    return hr;
}

HRESULT CudaH264::Encode(CUarray_st *cuArray)
{
    HRESULT hr = S_OK;
    const NvEncInputFrame *encoderInputFrame = pEnc->GetNextInputFrame();

    CUarray inputArray = (CUarray)encoderInputFrame;
    CUDA_ARRAY_DESCRIPTOR desc;
    memset((void*)&desc, 0, sizeof(CUDA_ARRAY_DESCRIPTOR));
    CUresult cuErr = cuArrayGetDescriptor(&desc, cuArray);

    NV_ENC_PIC_PARAMS encPicParams = {NV_ENC_PIC_PARAMS_VER};
    // Copy the CUDA array to the encoder input frame
    // Assume encoderInputFrame->inputPtr is a device pointer


    CUDA_MEMCPY2D copyParam = {0};
    copyParam.srcArray = cuArray;
    copyParam.srcMemoryType = CU_MEMORYTYPE_ARRAY; // Use CU_MEMORYTYPE_ARRAY for src
    copyParam.dstMemoryType = CU_MEMORYTYPE_DEVICE;
    copyParam.dstDevice = (CUdeviceptr)encoderInputFrame->inputPtr;
    copyParam.dstPitch = encoderInputFrame->pitch;
    copyParam.WidthInBytes = pEnc->GetEncodeWidth() * 4; // BGRA: 4 bytes per pixel
    copyParam.Height = pEnc->GetEncodeHeight();

    CUresult cudaStatus = cuMemcpy2D(&copyParam);
    if (cudaStatus != CUDA_SUCCESS)
    {
        std::cerr << "Failed to copy CUDA array to device memory. : cudaError : " << cudaStatus << std::endl;
        return E_FAIL;
    }
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
            pEnc->EndEncode(vPacket);
            WriteEncOutput();
            m_pEncBuf->Release();
            pEnc->DestroyEncoder();
            ZeroMemory(&initializeParams, sizeof(NV_ENC_INITIALIZE_PARAMS));
            ZeroMemory(&encodeConfig, sizeof(NV_ENC_CONFIG));
        }
        SAFE_RELEASE(m_pEncBuf);
        SAFE_RELEASE(pCtx);
    }
}

HRESULT CudaH264::Capture(int wait)
{
    HRESULT hr = pDDAWrapper->GetCapturedFrame(&pDupTex2D, wait);
    if (FAILED(hr))
        failCount++;

	if (!m_pEncBuf && pDupTex2D) {

		D3D11_TEXTURE2D_DESC targetDesc;
		ZeroMemory(&targetDesc, sizeof(targetDesc));
		pDupTex2D->GetDesc(&targetDesc);
		targetDesc.MiscFlags = 0;
        //targetDesc.Format = DXGI_FORMAT_NV12;

		hr = pD3DDev->CreateTexture2D(&targetDesc, nullptr, &m_pEncBuf);
	}

    if (pDupTex2D)
	{
		// copy pduptex2D to m_pEncBuf
		pCtx->CopyResource(m_pEncBuf, pDupTex2D); // pour copier pDupTex2D dans m_pEncBuf
	}

	return hr;
}

/// Write encoded video output to file
void CudaH264::WriteEncOutput()
{
    for (std::vector<uint8_t> &packet : vPacket)
    {
        fpOut.write(reinterpret_cast<char *>(packet.data()), packet.size());
    }
}

HRESULT CudaH264::Preproc()
{
    HRESULT hr = S_OK;
    size_t size;

    static bool isFirst = true;

    static CUgraphicsResource cuResource;
    static CUstream stream = 0;
    static CUarray_st *cuArray = nullptr;

    CUresult cudaStatus = CUDA_SUCCESS;

	if (isFirst) {
		isFirst = false;
		// Create CUDA stream
		cudaStatus = cuStreamCreate(&stream, CU_STREAM_DEFAULT);
		if (cudaStatus != CUDA_SUCCESS)
		{
			std::cerr << "Failed to create CUDA stream. Error code: " << cudaStatus << std::endl;
			return E_FAIL;
		}

        
        D3D11_TEXTURE2D_DESC desc;
        m_pEncBuf->GetDesc(&desc);
		cudaStatus = cuGraphicsD3D11RegisterResource(&cuResource, m_pEncBuf, CU_GRAPHICS_REGISTER_FLAGS_NONE);
		if (cudaStatus != CUDA_SUCCESS)
		{
			std::cerr << "Failed to register D3D11 resource with CUDA. : cudaError : " << cudaStatus << std::endl;
			return E_FAIL;
		}
	}

    // Map the resource for access by CUDA
    cudaStatus = cuGraphicsMapResources(1, &cuResource, stream);
    if (cudaStatus != CUDA_SUCCESS)
    {
        std::cerr << "Failed to map D3D11 resource to CUDA. Error code: " << cudaStatus << std::endl;
        cuGraphicsUnregisterResource(cuResource); // Cleanup before exit
        return E_FAIL;
    }

    // Get the CUDA array from the D3D11 resource
    unsigned int subResourceIndex = 0; // Typically 0 for the first subresource
    CUresult result = cuGraphicsSubResourceGetMappedArray(&cuArray, cuResource, subResourceIndex, 0);
    if (result != CUDA_SUCCESS)
    {
        // Handle error
        std::cerr << "Failed to get CUDA array from D3D11 resource. : cudaError : " << result << std::endl;
    }
    Encode(cuArray);
    cudaStatus = cuGraphicsUnmapResources(1, &cuResource, stream);
    if (cudaStatus != CUDA_SUCCESS)
    {
        std::cerr << "Failed to unmap D3D11 resource from CUDA. Error code: " << cudaStatus << std::endl;
        return E_FAIL;
    }
    returnIfError(hr);
    return hr;
}