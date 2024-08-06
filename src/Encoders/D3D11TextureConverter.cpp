#include "Defs.h"
#include "D3D11TextureConverter.h"

#define logError

/// Constructor
D3D11TextureConverter::D3D11TextureConverter(ID3D11Device *pDev, ID3D11DeviceContext *pCtx)
    : m_pDev(pDev)
    , m_pCtx(pCtx)
{
    m_pDev->AddRef();
    m_pCtx->AddRef();
}

/// Initialize Video Context
HRESULT D3D11TextureConverter::init()
{
    /// Obtain Video device and Video device context
    HRESULT hr = m_pDev->QueryInterface(__uuidof(ID3D11VideoDevice), (void**)&m_pVid);
    if (FAILED(hr))
    {
        logError("QAI for ID3D11VideoDevice failed, hr:{:x}", hr);
    }
    hr = m_pCtx->QueryInterface(__uuidof(ID3D11VideoContext), (void**)&m_pVidCtx);
    if (FAILED(hr))
    {
        logError("QAI for ID3D11VideoContext failed, hr:{:x}", hr);
    }

    return hr;
}

/// Release all Resources
void D3D11TextureConverter::cleanup()
{
    for (auto& it : m_viewMap)
    {
        ID3D11VideoProcessorOutputView* pVPOV = it.second;
        pVPOV->Release();
    }
    SAFE_RELEASE(m_pVP);
    SAFE_RELEASE(m_pVPEnum);
    SAFE_RELEASE(m_pVidCtx);
    SAFE_RELEASE(m_pVid);
    SAFE_RELEASE(m_pCtx);
    SAFE_RELEASE(m_pDev);
}


/// Perform texture conversion
HRESULT D3D11TextureConverter::convert(ID3D11Texture2D* srcTexture, ID3D11Texture2D*dstTexture)
{
    HRESULT hr = S_OK;
    ID3D11VideoProcessorInputView* pVPIn = nullptr;

    D3D11_TEXTURE2D_DESC inDesc = { 0 };
    D3D11_TEXTURE2D_DESC outDesc = { 0 };
    srcTexture->GetDesc(&inDesc);
    dstTexture->GetDesc(&outDesc);

    /// Check if VideoProcessor needs to be reconfigured
    /// Reconfiguration is required if input/output dimensions have changed
    if (m_pVP)
    {
        if (m_inDesc.Width != inDesc.Width ||
            m_inDesc.Height != inDesc.Height ||
            m_outDesc.Width != outDesc.Width ||
            m_outDesc.Height != outDesc.Height)
        {
            SAFE_RELEASE(m_pVPEnum);
            SAFE_RELEASE(m_pVP);
        }
    }

    if (!m_pVP)
    {
        /// Initialize Video Processor
        m_inDesc = inDesc;
        m_outDesc = outDesc;
        D3D11_VIDEO_PROCESSOR_CONTENT_DESC contentDesc =
        {
            D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE,
            { 1, 1 }, inDesc.Width, inDesc.Height,
            { 1, 1 }, outDesc.Width, outDesc.Height,
            D3D11_VIDEO_USAGE_PLAYBACK_NORMAL
        };
        hr = m_pVid->CreateVideoProcessorEnumerator(&contentDesc, &m_pVPEnum);;
        if (FAILED(hr))
        {
            logError("CreateVideoProcessorEnumerator failed, hr:{:x}", hr);
            return hr;
        }
        hr = m_pVid->CreateVideoProcessor(m_pVPEnum, 0, &m_pVP);;
        if (FAILED(hr))
        {
            logError("CreateVideoProcessor failed, hr:{:x}", hr);
            return hr;
        }
    }

    /// Obtain Video Processor Input view from input texture
    D3D11_VIDEO_PROCESSOR_INPUT_VIEW_DESC inputVD = { 0, D3D11_VPIV_DIMENSION_TEXTURE2D,{ 0,0 } };
    hr = m_pVid->CreateVideoProcessorInputView(srcTexture, m_pVPEnum, &inputVD, &pVPIn);
    if (FAILED(hr))
    {
        logError("CreateVideoProcessInputView failed, hr:{:x}", hr);
        return hr;
    }

    /// Obtain Video Processor Output view from output texture
    ID3D11VideoProcessorOutputView* pVPOV = nullptr;
    auto it = m_viewMap.find(dstTexture);
    /// Optimization: Check if we already created a video processor output view for this texture
    if (it == m_viewMap.end())
    {
        /// We don't have a video processor output view for this texture, create one now.
        D3D11_VIDEO_PROCESSOR_OUTPUT_VIEW_DESC ovD = { D3D11_VPOV_DIMENSION_TEXTURE2D };
        hr = m_pVid->CreateVideoProcessorOutputView(dstTexture, m_pVPEnum, &ovD, &pVPOV);
        if (FAILED(hr))
        {
            SAFE_RELEASE(pVPIn);
            logError("CreateVideoProcessorOutputView, hr:{:x}", hr);
            return hr;
        }
        m_viewMap.insert({ dstTexture, pVPOV });
    }
    else
    {
        pVPOV = it->second;
    }

    /// Create a Video Processor Stream to run the operation
    D3D11_VIDEO_PROCESSOR_STREAM stream = { TRUE, 0, 0, 0, 0, nullptr, pVPIn, nullptr };

    /// Perform the Colorspace conversion
    hr = m_pVidCtx->VideoProcessorBlt(m_pVP, pVPOV, 0, 1, &stream);
    if (FAILED(hr))
    {
        SAFE_RELEASE(pVPIn);
        logError("VideoProcessorBlt failed, hr:{:x}", hr);
        return hr;
    }
    SAFE_RELEASE(pVPIn);
    return hr;
}

