
cbuffer cbImmutable : register(b0)
{
	uint g_InWidth;
	uint g_InHght;
	uint g_DtxAddressOffset;
	uint g_DtyAddressOffset;
};

cbuffer cbscale : register(b1)
{
	float scale;
};

StructuredBuffer<float2>	g_Input: register(t0);
Texture2D	lookup_i_wr_wi	: register(t1); // FFT wave displacement map in VS
Texture2D	real_tex	: register(t2); // FFT wave displacement map in VS
Texture2D	imag_tex	: register(t3); // FFT wave displacement map in VS
SamplerState point_sampler	: register(s0);

struct VS_QUAD_OUTPUT
{
	float4 Position		: SV_POSITION;	// vertex position
	float2 TexCoord		: TEXCOORD0;	// vertex texture coords 
};

VS_QUAD_OUTPUT QuadVS(float4 vPos : POSITION)
{
	VS_QUAD_OUTPUT Output;

	Output.Position = vPos;
	Output.TexCoord.x = 0.5f + vPos.x * 0.5f;
	Output.TexCoord.y = 0.5f - vPos.y * 0.5f;

	return Output;
}

void FFTUnit_1(float2 r1, float2 r2, float wr, float wi,
	out float4 oReal, out float4 oImag)
{
	uint index_x = (uint)(r1.x * (float)g_InWidth);
	uint index_y = (uint)(r1.y * (float)g_InHght);
	int    htindex1 = (index_y*g_InWidth + index_x);
	float2 ht1 = g_Input[htindex1];
	
	int    dxindex1 = htindex1 + g_DtxAddressOffset;
	float2 dx1 = g_Input[dxindex1];

	int    dyindex1 = htindex1 + g_DtyAddressOffset;
	float2 dy1 = g_Input[dyindex1];

	index_x = (uint)(r2.x * (float)g_InWidth);
	index_y = (uint)(r2.y * (float)g_InHght);
	int    htindex2 = (index_y*g_InWidth + index_x);
	float2 ht2 = g_Input[htindex2];

	int    dxindex2 = htindex2 + g_DtxAddressOffset;
	float2 dx2 = g_Input[dxindex2];

	int    dyindex2 = htindex2 + g_DtyAddressOffset;
	float2 dy2 = g_Input[dyindex2];

	float4 input_r1 = float4(ht1.x, dx1.x, dy1.x,0);
	float4 input_i1 = float4(ht1.y, dx1.y, dy1.y,0);

	float4 input_r2 = float4(ht2.x, dx2.x, dy2.x,0);
	float4 input_i2 = float4(ht2.y, dx2.y, dy2.y,0);

	//float4 input_r1 = real_tex.Sample(point_sampler, r1);
	//float4 input_i1 = imag_tex.Sample(point_sampler, r1);

	//float4 input_r2 = real_tex.Sample(point_sampler, r2);
	//float4 input_i2 = imag_tex.Sample(point_sampler, r2);

	float4 res_r = wr * input_r2 - wi * input_i2;
	float4 res_i = wi * input_r2 + wr * input_i2;

	oReal = input_r1 + res_r;
	oImag = input_i1 + res_i;
}

void FFTXPS_1(VS_QUAD_OUTPUT In,
	out float4 oReal : SV_Target0,
	out float4 oImag : SV_Target1)
{
	float4 i_wr_wi = lookup_i_wr_wi.Sample(point_sampler, In.TexCoord);

	float2 r1 = float2(i_wr_wi.x, In.TexCoord.y);
	float2 r2 = float2(i_wr_wi.y, In.TexCoord.y);

	FFTUnit_1(r1, r2, i_wr_wi.z, i_wr_wi.w,
		oReal, oImag);
}

void FFTYPS_1(float2 tc : TEXCOORD0,
	out float4 oReal : SV_Target0,
	out float4 oImag : SV_Target1)
{
	float4 i_wr_wi = lookup_i_wr_wi.Sample(point_sampler, tc);

	float2 r1 = float2(tc.x, i_wr_wi.x);
	float2 r2 = float2(tc.x, i_wr_wi.y);

	FFTUnit_1(r1, r2, i_wr_wi.z, i_wr_wi.w,
		oReal, oImag);
}

void FFTUnit(float2 r1, float2 r2, float wr, float wi,
	out float4 oReal, out float4 oImag)
{
	float4 input_r1 = real_tex.Sample(point_sampler, r1);
	float4 input_i1 = imag_tex.Sample(point_sampler, r1);

	float4 input_r2 = real_tex.Sample(point_sampler, r2);
	float4 input_i2 = imag_tex.Sample(point_sampler, r2);

	float4 res_r = wr * input_r2 - wi * input_i2;
	float4 res_i = wi * input_r2 + wr * input_i2;

	oReal = input_r1 + res_r;
	oImag = input_i1 + res_i;
}

void FFTXPS(VS_QUAD_OUTPUT In ,
	out float4 oReal : SV_Target0,
	out float4 oImag : SV_Target1)
{
	float4 i_wr_wi = lookup_i_wr_wi.Sample(point_sampler, In.TexCoord);

	float2 r1 = float2(i_wr_wi.x, In.TexCoord.y);
	float2 r2 = float2(i_wr_wi.y, In.TexCoord.y);

	FFTUnit(r1, r2, i_wr_wi.z, i_wr_wi.w,
		oReal, oImag);
}

void FFTYPS(VS_QUAD_OUTPUT In,
	out float4 oReal : SV_Target0,
	out float4 oImag : SV_Target1)
{
	float4 i_wr_wi = lookup_i_wr_wi.Sample(point_sampler, In.TexCoord);

	float2 r1 = float2(In.TexCoord.x, i_wr_wi.x);
	float2 r2 = float2(In.TexCoord.x, i_wr_wi.y);

	FFTUnit(r1, r2, i_wr_wi.z, i_wr_wi.w,
		oReal, oImag);

	//if (scale > 0)
	{
		//oReal *= scale;
		//oImag *= scale;
	}
}