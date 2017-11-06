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

#define TableResolution 512.f
#define PI 3.1415926f
#define stepNum 1024
RWTexture2D<float4> g_OutputTex : register(u0);


float Gaussian(float v, float r)
{
	return 1.0f / sqrt(2.0f * PI * v) * exp(-(r * r) / (2.0f * v));
}

float3 Scatter(float r)
{
	return Gaussian(0.0064f * 1.414f, r) *float3(0.233f, 0.455f, 0.649f) +
		   Gaussian(0.0484f * 1.414f, r) *float3(0.100f, 0.366f, 0.344f) +
		   Gaussian(0.187f * 1.414f, r) *float3(0.118f, 0.198f, 0.0f) +
		   Gaussian(0.567f * 1.414f, r) *float3(0.113f, 0.007f, 0.007f) +
		   Gaussian(1.99f * 1.414f, r) *float3(0.358f, 0.004f, 0.0f) +
		   Gaussian(7.41f * 1.414f, r) *float3(0.078f, 0.0f, 0.0f);
}

float3 PreSSS(float costhata, float r)
{
	float thata = acos(costhata);
	float a = -(PI);
	float inc = PI/64.0;
	float3 totallinght = 0;
	float3  totalw = 0;
	//while(a <= PI)
	while(a<= PI)
	{
		float  sampleangler = thata + a;
		float  diffuse = saturate(cos(sampleangler));
		float  sampleDist = abs(2.0f * r * sin(a*0.5f));
		float3 weights = Scatter(sampleDist);
		totallinght += diffuse*weights;
		totalw += weights;
		a += inc;
	}

	return max(costhata,totallinght / totalw);
}
//---------------------------------------- Compute Shaders -----------------------------------------

// Pre-FFT data preparation:

// Notice: In CS5.0, we can output up to 8 RWBuffers but in CS4.x only one output buffer is allowed,
// that way we have to allocate one big buffer and manage the offsets manually. The restriction is
// not caused by NVIDIA GPUs and does not present on NVIDIA GPUs when using other computing APIs like
// CUDA and OpenCL.

// H(0) -> H(t)
[numthreads(BLOCK_SIZE_X, BLOCK_SIZE_Y, 1)]
void PreSSSCS(uint3 DTid : SV_DispatchThreadID)
{ 
	float costhata = DTid.x * 2.0  / TableResolution -1.0f;
	float r = lerp(0.7, 4.5, DTid.y / TableResolution);
	float3 color = PreSSS(costhata,r);
	g_OutputTex[DTid.xy] =  float4(color, 1);// *rcp(g_Num * 2);
}


