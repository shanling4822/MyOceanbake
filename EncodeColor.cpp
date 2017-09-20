#include "stdafx.h"
#include "EncodeColor.h"
#include <math.h>
#include <algorithm>  
#include <d3dx9math.h>
const float EncodeColor::LogScale = 11.5f;
const float EncodeColor::LogBlackPoint = pow(2.0f, -0.5f * LogScale);
#define DELTA			(0.00001f)
void EncodeColor::GetLUVW(const float RGB[3], float& L, float& U, float& V, float& W)
{
	float R = max(0.0f, RGB[0]);
	float G = max(0.0f, RGB[1]);
	float B = max(0.0f, RGB[2]);

	L = max(R,max(G,B));
	if (L < 1e-4f)
	{
		U = 1.0f;
		V = 1.0f;
		W = 1.0f;
	}
	else
	{
		U = R / L;
		V = G / L;
		W = B / L;
	}
}

void EncodeColor::GetDxyz(unsigned char L, unsigned char U, unsigned char V, unsigned char W, float MinValue, float LogScale, float RGB[3])
{
	float pow2 = pow(2, (float)L / 255 / LogScale) - LogBlackPoint;
	RGB[0] = (float)U / 255 * pow2 + MinValue;
	RGB[1] = (float)V / 255 * pow2 + MinValue;
	RGB[2] = (float)W / 255 * pow2 + MinValue;
}

void EncodeColor::ComputeCoefficient(float MinValue, float MaxValue, float &LogScale)
{
	float Max = MaxValue - MinValue;
	float log2 = log2f(Max + LogBlackPoint);
	LogScale = 1 / log2;
	/*int i = 0;
	do
	{
		int scale = 1 << i;
		if (log2 * scale > 255)
			break;
		else
			LogScale = scale;

		++i;
	} while (true);*/
}
float rand(float n, float &i) { return 0.5f > modf(n, &i) ? n : n + 1; }

void EncodeColor::Encode(unsigned char *des, float *src, unsigned int size, float MinValue, float MaxValue, float LogScale)
{
	float RGB[3];
	float L, U, V, W;
	float x,z;
	for (int i = 0; i < size; i += 4)
	{
		z = src[i + 2];
		RGB[0] = src[i] - MinValue;
		RGB[1] = src[i + 1] - MinValue;
		RGB[2] = src[i + 2] - MinValue;

		GetLUVW(RGB, L, U, V, W);

		float log2 = log2f(L + LogBlackPoint);
		log2 *= LogScale;

		des[i] = (unsigned char)min(max(rand(U * 255, x), 0), 255);
		des[i+1] = (unsigned char)min(max(rand(V * 255, x), 0), 255);
		des[i+2] = (unsigned char)min(max(rand(W * 255, x), 0), 255);
		des[i + 3] = (unsigned char)min(max(rand(log2 * 255, x), 0), 255);

		GetDxyz(des[i + 3], des[i], des[i + 1], des[i + 2], MinValue, LogScale, RGB);
	}
}

void EncodeColor::Encode(unsigned char *des, unsigned char *error, float *src, unsigned int size, float MinValue, float MaxValue, float LogScale)
{
	float RGB[3];
	float L, U, V, W;
	float x, z;
	for (int i = 0; i < size; i += 4)
	{
		z = src[i + 2];
		RGB[0] = src[i] - MinValue;
		RGB[1] = src[i + 1] - MinValue;
		RGB[2] = src[i + 2] - MinValue;

		GetLUVW(RGB, L, U, V, W);

		float log2 = log2f(L + LogBlackPoint);
		log2 *= LogScale;

		des[i]     = (unsigned char)min(max((U * 255 ), 0), 255);
		des[i + 1] = (unsigned char)min(max((V * 255), 0), 255);
		des[i + 2] = (unsigned char)min(max((W * 255), 0), 255);
		des[i + 3] = (unsigned char)min(max((log2 * 255), 0), 255);

		error[i] = (unsigned char)((U * 255.f - des[i]) * 255);
		error[i + 1] = (unsigned char)((V * 255.f - des[i + 1]) * 255);
		error[i + 2] = (unsigned char)((W * 255.f - des[i + 2]) * 255);
		error[i + 3] = (unsigned char)((log2 * 255.f - des[i + 3]) * 255);

		//GetDxyz(des[i + 3], des[i], des[i + 1], des[i + 2], MinValue, LogScale, RGB);
	}
}