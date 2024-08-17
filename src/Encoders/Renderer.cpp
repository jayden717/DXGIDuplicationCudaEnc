#undef D3D11_NO_HELPERS
#include "Renderer.h"

#include <array>

#define USE_EXTERNEL_DEVICE 1

int32_t Renderer::Init(HWND window, ID3D11Device* deviceApi)
{
    // m_viewport
    RECT WindowRect = {};
    GetClientRect(window, &WindowRect);

    int ClientWidth = WindowRect.right - WindowRect.left;
    int ClientHeight = WindowRect.bottom - WindowRect.top;

    m_viewport = {
        0.0f, 0.0f,
        (float)ClientWidth, (float)ClientHeight,
        0.0f, 1.0f
    };
    HRESULT result;
#if USE_EXTERNEL_DEVICE
	ID3D11Device* baseDevice = deviceApi;
	ID3D11DeviceContext* baseContext = nullptr;
	baseDevice->GetImmediateContext(&baseContext);
#else
	ID3D11Device* baseDevice = nullptr;
	ID3D11DeviceContext* baseContext = nullptr;

    UINT CreationFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#ifdef _DEBUG
	CreationFlags = D3D11_CREATE_DEVICE_DEBUG;
#endif

	D3D_FEATURE_LEVEL FeatureLevels[] = {
		D3D_FEATURE_LEVEL_11_0
	};

	result = D3D11CreateDevice(0, D3D_DRIVER_TYPE_HARDWARE, 0,
		CreationFlags, FeatureLevels,
		ARRAYSIZE(FeatureLevels),
		D3D11_SDK_VERSION, &baseDevice, 0,
		&baseContext);

	if (FAILED(result)) {
		MessageBox(0, L"D3D11CreateDevice failed", 0, 0);
		return GetLastError();
	}
#endif

	result = baseDevice->QueryInterface(__uuidof(ID3D11Device1), (void**)&m_device);
    assert(SUCCEEDED(result));

    result = baseContext->QueryInterface(__uuidof(ID3D11DeviceContext1), (void**)&m_context);
    assert(SUCCEEDED(result));
    baseContext->Release();

    // Swap chain
    DXGI_SWAP_CHAIN_DESC1 wwapChainDesc = {};
    wwapChainDesc.Width = 0;
    wwapChainDesc.Height = 0;
    wwapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    wwapChainDesc.SampleDesc.Count = 1;
    wwapChainDesc.SampleDesc.Quality = 0;
    wwapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    wwapChainDesc.BufferCount = 1;
    wwapChainDesc.Scaling = DXGI_SCALING_STRETCH;
    wwapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    wwapChainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
    wwapChainDesc.Flags = 0;

    IDXGIDevice2* dxgiDevice;
    result = m_device->QueryInterface(__uuidof(IDXGIDevice2), (void**)&dxgiDevice);
    assert(SUCCEEDED(result));

    IDXGIAdapter* dxgiAdapter;
    result = dxgiDevice->GetAdapter(&dxgiAdapter);
    assert(SUCCEEDED(result));
    dxgiDevice->Release();

    IDXGIFactory2* dxgiFactory;
    result = dxgiAdapter->GetParent(__uuidof(IDXGIFactory2), (void**)&dxgiFactory);
    assert(SUCCEEDED(result));
    dxgiAdapter->Release();

    result = dxgiFactory->CreateSwapChainForHwnd(m_device, window, &wwapChainDesc, 0, 0, &m_swapChain);
    assert(SUCCEEDED(result));
    dxgiFactory->Release();

    // Frame buffer
    ID3D11Texture2D* frameBuffer;
    result = m_swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&frameBuffer);
    assert(SUCCEEDED(result));

    result = m_device->CreateRenderTargetView(frameBuffer, 0, &m_frameBufferView);
    assert(SUCCEEDED(result));
    frameBuffer->Release();

    // Shaders
    ID3D10Blob* vsBlob;
    result = D3DCompileFromFile(L"shaders/VertexShader.hlsl", 0, 0, "vs_main",
        "vs_5_0", 0, 0, &vsBlob, 0);
    assert(SUCCEEDED(result));

    result = m_device->CreateVertexShader(vsBlob->GetBufferPointer(),
        vsBlob->GetBufferSize(),
        0,
        &m_vertexShader);
    assert(SUCCEEDED(result));

    ID3D10Blob* rgbPsBlob;
    result = D3DCompileFromFile(L"shaders/PixelShaderRGB.hlsl", 0, 0, "ps_main",
        "ps_5_0", 0, 0, &rgbPsBlob, 0);
    assert(SUCCEEDED(result));

    result = m_device->CreatePixelShader(rgbPsBlob->GetBufferPointer(),
        rgbPsBlob->GetBufferSize(),
        0,
        &m_rgbPixelShader);
    assert(SUCCEEDED(result));
    rgbPsBlob->Release();

    ID3D10Blob* nv12PsBlob;
    result = D3DCompileFromFile(L"shaders/PixelShaderNV12.hlsl", 0, 0, "ps_main",
        "ps_5_0", 0, 0, &nv12PsBlob, 0);
    assert(SUCCEEDED(result));

    result = m_device->CreatePixelShader(nv12PsBlob->GetBufferPointer(),
        nv12PsBlob->GetBufferSize(),
        0,
        &m_nv12PixelShader);
    assert(SUCCEEDED(result));
    nv12PsBlob->Release();

    // Input layout

    D3D11_INPUT_ELEMENT_DESC inputElementDesc[] = {
        {
            "POSITION", 0,
            DXGI_FORMAT_R32G32B32_FLOAT,
            0, 0,
            D3D11_INPUT_PER_VERTEX_DATA, 0
        },
        {
            "TEXCOORD", 0,
            DXGI_FORMAT_R32G32_FLOAT,
            0, D3D11_APPEND_ALIGNED_ELEMENT,
            D3D11_INPUT_PER_VERTEX_DATA, 0
        }
    };

    result = m_device->CreateInputLayout(inputElementDesc,
        ARRAYSIZE(inputElementDesc),
        vsBlob->GetBufferPointer(),
        vsBlob->GetBufferSize(),
        &m_inputLayout
    );
    assert(SUCCEEDED(result));
    vsBlob->Release();

    
    // Vertex buffer
    D3D11_BUFFER_DESC vertexDataDesc = {
        sizeof(VertexData),
        D3D11_USAGE_DEFAULT,
        D3D11_BIND_VERTEX_BUFFER,
        0, 0, 0
    };

    D3D11_SUBRESOURCE_DATA vertexDataInitial = { VertexData };
    result = m_device->CreateBuffer(&vertexDataDesc,
        &vertexDataInitial,
        &m_vertexDataBuffer);
    assert(SUCCEEDED(result));

    Stride = 5 * sizeof(float);
    Offset = 0;
    NumVertices = sizeof(VertexData) / Stride;

    // Sampler
    D3D11_SAMPLER_DESC imageSamplerDesc = {};

    imageSamplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    imageSamplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    imageSamplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    imageSamplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    imageSamplerDesc.MipLODBias = 0.0f;
    imageSamplerDesc.MaxAnisotropy = 1;
    imageSamplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    imageSamplerDesc.BorderColor[0] = 1.0f;
    imageSamplerDesc.BorderColor[1] = 1.0f;
    imageSamplerDesc.BorderColor[2] = 1.0f;
    imageSamplerDesc.BorderColor[3] = 1.0f;
    imageSamplerDesc.MinLOD = -FLT_MAX;
    imageSamplerDesc.MaxLOD = FLT_MAX;

    result = m_device->CreateSamplerState(&imageSamplerDesc, &m_imageSamplerState);
    assert(SUCCEEDED(result));

    return 0;
}

void Renderer::RenderNV12(ID3D11Texture2D* texture)
{
    // Shader resource view
    D3D11_TEXTURE2D_DESC textureDesc;
    texture->GetDesc(&textureDesc);

    /*
	ID3D11Resource* resource;
	HRESULT hr = texture->QueryInterface(__uuidof(ID3D11Resource), (void**)&resource);
	hr = DirectX::SaveDDSTextureToFile(m_context, resource, L"D:\\render.dds");
    */

    ID3D11ShaderResourceView* luminanceView;
    ID3D11ShaderResourceView* chrominanceView;

    D3D11_SHADER_RESOURCE_VIEW_DESC const luminancePlaneDesc = CD3D11_SHADER_RESOURCE_VIEW_DESC(
        texture,
        D3D11_SRV_DIMENSION_TEXTURE2D,
        DXGI_FORMAT_R8_UNORM
    );

    HRESULT hr = m_device->CreateShaderResourceView(
        texture,
        &luminancePlaneDesc,
        &luminanceView
    );
	assert(SUCCEEDED(hr));

    D3D11_SHADER_RESOURCE_VIEW_DESC const chrominancePlaneDesc = CD3D11_SHADER_RESOURCE_VIEW_DESC(
        texture,
        D3D11_SRV_DIMENSION_TEXTURE2D,
        DXGI_FORMAT_R8G8_UNORM
    );

    hr = m_device->CreateShaderResourceView(
        texture,
        &chrominancePlaneDesc,
        &chrominanceView
    );
	assert(SUCCEEDED(hr));

    // Rendering NV12 requires two resource views, which represent the luminance and chrominance channels of the YUV formatted texture.
    std::array<ID3D11ShaderResourceView*, 2> const textureViews = {
        luminanceView,
        chrominanceView
    };

	float color[4] = { 1.0f, 0.0f, 0.0f, 1.0f };

    m_context->ClearRenderTargetView(m_frameBufferView, color);

    m_context->RSSetViewports(1, &m_viewport);

    m_context->OMSetRenderTargets(1, &m_frameBufferView, 0);

    m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_context->IASetInputLayout(m_inputLayout);

    m_context->VSSetShader(m_vertexShader, 0, 0);
    m_context->PSSetShader(m_nv12PixelShader, 0, 0);

    //m_context->PSSetShaderResources(0, 1, &m_imageShaderResourceView);

    // Bind the NV12 channels to the shader.
    m_context->PSSetShaderResources(
        0,
        textureViews.size(),
        textureViews.data()
    );

    m_context->PSSetSamplers(0, 1, &m_imageSamplerState);

    m_context->IASetVertexBuffers(0, 1, &m_vertexDataBuffer, &Stride, &Offset);

    m_context->Draw(NumVertices, 0);

    m_swapChain->Present(1, 0);

    luminanceView->Release();
    chrominanceView->Release();

    //m_imageShaderResourceView->Release();
}

void Renderer::RenderRGB(ID3D11Texture2D* texture)
{
    // Shader resource view
    D3D11_TEXTURE2D_DESC textureDesc;
    texture->GetDesc(&textureDesc);

    /*
    ID3D11Resource* resource;
    HRESULT hr = texture->QueryInterface(__uuidof(ID3D11Resource), (void**)&resource);
    hr = DirectX::SaveDDSTextureToFile(m_context, resource, L"D:\\render.dds");
    */

    HRESULT hr = m_device->CreateShaderResourceView(
        texture,
        nullptr,
        &m_imageShaderResourceView
    );
    assert(SUCCEEDED(hr));


    float color[4] = { 1.0f, 0.0f, 0.0f, 1.0f };

    m_context->ClearRenderTargetView(m_frameBufferView, color);

    m_context->RSSetViewports(1, &m_viewport);

    m_context->OMSetRenderTargets(1, &m_frameBufferView, 0);

    m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_context->IASetInputLayout(m_inputLayout);

    m_context->VSSetShader(m_vertexShader, 0, 0);
    m_context->PSSetShader(m_rgbPixelShader, 0, 0);

    m_context->PSSetShaderResources(0, 1, &m_imageShaderResourceView);

    m_context->PSSetSamplers(0, 1, &m_imageSamplerState);

    m_context->IASetVertexBuffers(0, 1, &m_vertexDataBuffer, &Stride, &Offset);

    m_context->Draw(NumVertices, 0);

    m_swapChain->Present(1, 0);

    m_imageShaderResourceView->Release();
}
