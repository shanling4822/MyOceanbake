// Copyright (c) 2011 NVIDIA Corporation. All rights reserved.
//
// TO  THE MAXIMUM  EXTENT PERMITTED  BY APPLICABLE  LAW, THIS SOFTWARE  IS PROVIDED
// *AS IS*  AND NVIDIA AND  ITS SUPPLIERS DISCLAIM  ALL WARRANTIES,  EITHER  EXPRESS
// OR IMPLIED, INCLUDING, BUT NOT LIMITED  TO, NONINFRINGEMENT,IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.  IN NO EVENT SHALL  NVIDIA 
// OR ITS SUPPLIERS BE  LIABLE  FOR  ANY  DIRECT, SPECIAL,  INCIDENTAL,  INDIRECT,  OR  
// CONSEQUENTIAL DAMAGES WHATSOEVER (INCLUDING, WITHOUT LIMITATION,  DAMAGES FOR LOSS 
// OF BUSINESS PROFITS, BUSINESS INTERRUPTION, LOSS OF BUSINESS INFORMATION, OR ANY 
// OTHER PECUNIARY LOSS) ARISING OUT OF THE  USE OF OR INABILITY  TO USE THIS SOFTWARE, 
// EVEN IF NVIDIA HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGES.
//
// Please direct any bugs or questions to SDKFeedback@nvidia.com
#define BLOCK_SIZE_X 16
#define BLOCK_SIZE_Y 16

cbuffer cbImmutable : register(b0)
{
	uint g_Num;
	
};
struct Data
{
	float2 pos;
	float2 wave;

};


StructuredBuffer<Data> gInputA	: register(t0);
RWTexture2D<float4> g_OutputTex : register(u0);



//---------------------------------------- Compute Shaders -----------------------------------------

// Pre-FFT data preparation:

// Notice: In CS5.0, we can output up to 8 RWBuffers but in CS4.x only one output buffer is allowed,
// that way we have to allocate one big buffer and manage the offsets manually. The restriction is
// not caused by NVIDIA GPUs and does not present on NVIDIA GPUs when using other computing APIs like
// CUDA and OpenCL.

// H(0) -> H(t)
[numthreads(BLOCK_SIZE_X, BLOCK_SIZE_Y, 1)]
void SinbasenoiseCS(uint3 DTid : SV_DispatchThreadID)
{ 
	float total = 0;
	for (uint i = 0; i < g_Num; ++i)
	{
		float2 temp1 = DTid.xy - gInputA[i].pos;
		float  temp2 = sqrt(dot(temp1, temp1));
		float  temp3 = 1 / (2.08 + 5 * gInputA[i].wave.x);// rcp(2.08 + 5 * gInputA[i].wave.x);
		float  tempsin = 0;
		float  tempcos = 0;
		sincos(temp2*temp3, tempsin, tempcos);

		total += abs(tempsin);
	}
	float3 color = (total / g_Num).xxx;
	g_OutputTex[DTid.xy] = float4(color,1);// *rcp(g_Num * 2);
}


