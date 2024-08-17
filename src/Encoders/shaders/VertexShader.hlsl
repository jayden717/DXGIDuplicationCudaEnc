//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************


// Per-vertex data used as input to the vertex shader.
struct VertexShaderInput
{
    min16float3 pos      : POSITION0;
    min16float2 texCoord : TEXCOORD0;
};

// Per-vertex data passed to the geometry shader.
struct VertexShaderOutput
{
    min16float4 pos      : SV_POSITION;
    min16float2 texCoord : TEXCOORD0;
};

// Simple shader to do vertex processing on the GPU.
VertexShaderOutput vs_main(VertexShaderInput input)
{
    VertexShaderOutput output;
    float4 pos = float4(input.pos, 1.0f);
    output.pos = (min16float4)pos;
    output.texCoord = (min16float2)input.texCoord;

    return output;
}
