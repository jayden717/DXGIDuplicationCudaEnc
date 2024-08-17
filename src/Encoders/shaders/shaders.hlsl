struct VS_Input
{
	float2 position: POSITION;
	float2 uv: UV;
};

struct VS_Output
{
	float4 position: SV_POSITION;
	float2 uv: UV;
};

VS_Output vs_main(VS_Input input)
{
	VS_Output output;
	output.position = float4(input.position, 0.0f, 1.0f);
	output.uv = input.uv;
	return output;
};

Texture2D<float>  luminanceChannel   : t0;
Texture2D<float2> chrominanceChannel : t1;
SamplerState      defaultSampler     : s0;

// Derived from https://msdn.microsoft.com/en-us/library/windows/desktop/dd206750(v=vs.85).aspx
// Section: Converting 8-bit YUV to RGB888
static const float3x3 YUVtoRGBCoeffMatrix = 
{
    1.164383f,  1.164383f, 1.164383f,
    0.000000f, -0.391762f, 2.017232f,
    1.596027f, -0.812968f, 0.000000f
};

float3 ConvertYUVtoRGB(float3 yuv)
{
    // Derived from https://msdn.microsoft.com/en-us/library/windows/desktop/dd206750(v=vs.85).aspx
    // Section: Converting 8-bit YUV to RGB888

    // These values are calculated from (16 / 255) and (128 / 255)
    yuv -= float3(0.062745f, 0.501960f, 0.501960f);
    yuv = mul(yuv, YUVtoRGBCoeffMatrix);

    return saturate(yuv);
}

min16float4 ps_main(VS_Output input) : SV_TARGET
{
    float y = luminanceChannel.Sample(defaultSampler, input.uv);
    float2 uv = chrominanceChannel.Sample(defaultSampler, input.uv);

    return min16float4(ConvertYUVtoRGB(float3(y, uv)), 1.f);

	/*
	float3 yuv;
	float4 rgba;
	yuv.x = luminanceChannel.Sample(defaultSampler, input.uv);
	yuv.yz = chrominanceChannel.Sample(defaultSampler, input.uv);
	yuv.x = 1.164383561643836 * (yuv.x - 0.0625);
	yuv.y = yuv.y - 0.5;
	yuv.z = yuv.z - 0.5;
	rgba.x = saturate(yuv.x + 1.792741071428571 * yuv.z);
	rgba.y = saturate(yuv.x - 0.532909328559444 * yuv.z - 0.21324861427373 * yuv.y);
	rgba.z = saturate(yuv.x + 2.112401785714286 * yuv.y);
	rgba.a = 1.0f;
	return rgba;
	*/
}
