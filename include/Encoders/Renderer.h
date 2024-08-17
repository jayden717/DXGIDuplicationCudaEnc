#pragma once
#include <assert.h>
#include <d3d11_1.h>
#include <d3dcompiler.h>
#include <float.h>
#include <cstdint>

class Renderer
{
public:
    int32_t Init(HWND window, ID3D11Device* device);
    void RenderNV12(ID3D11Texture2D* texture);
    void RenderRGB(ID3D11Texture2D* texture);

    ID3D11Device1* GetDevice() {
        return m_device;
    }

private:
    UINT Stride = 0;
    UINT Offset = 0;
    UINT NumVertices = 0;

    // m_viewport
    D3D11_VIEWPORT m_viewport;

    // m_device & m_context
    ID3D11Device1* m_device;
    ID3D11DeviceContext1* m_context;

    // Swap chain
    IDXGISwapChain1* m_swapChain;

    ID3D11RenderTargetView* m_frameBufferView;
    ID3D11InputLayout* m_inputLayout;
    ID3D11VertexShader* m_vertexShader;
    ID3D11PixelShader* m_rgbPixelShader;
    ID3D11PixelShader* m_nv12PixelShader;

    // Shader resource view
    ID3D11ShaderResourceView* m_imageShaderResourceView;
    ID3D11SamplerState* m_imageSamplerState;

    // Vertex data, x,y,z position & uv coords
    float VertexData[30] = {
        -1.0f, -1.0f, 0.0f, 0.0f, 1.0f,
        -1.0f, 1.0f,  0.0f, 0.0f, 0.0f,
        1.0f, 1.0f,   0.0f, 1.0f, 0.0f,
        -1.0f, -1.0f, 0.0f, 0.0f, 1.0f,
        1.0f, 1.0f,   0.0f, 1.0f, 0.0f,
        1.0f, -1.0f,  0.0f, 1.0f, 1.0f
    };

    ID3D11Buffer* m_vertexDataBuffer;
};

