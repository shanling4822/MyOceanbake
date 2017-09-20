#pragma once
#include <vector>

class EncodeColor
{
public:
	static void Encode(unsigned char *des, float *src, unsigned int size, float MinValue, float MaxValue, float LogScale);
	static void Encode(unsigned char *des, unsigned char *error, float *src, unsigned int size, float MinValue, float MaxValue, float LogScale);
	static void ComputeCoefficient(float MinValue, float MaxValue, float &LogScale);
	static void GetLUVW(const float RGB[3], float& L, float& U, float& V, float& W);

	static void GetDxyz(unsigned char L, unsigned char U, unsigned char V, unsigned char W, float MinValue, float LogScale, float RGB[3] );
protected:
	static const float LogScale;
	static const float LogBlackPoint;
};