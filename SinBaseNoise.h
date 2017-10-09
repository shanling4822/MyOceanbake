#pragma once
#include <d3dx9math.h>
#include <vector>

struct TSineBaseNode
{
	float x;
	float y;
	float wave;
	float temp;
};

class SinBaseNoise
{
public:
	SinBaseNoise();
	~SinBaseNoise()
	{
		SAFE_RELEASE(m_pSRV_SineBase);
		SAFE_RELEASE(m_pUAV_SineBase);
		SAFE_RELEASE(m_pBuff_SineBase);

		SAFE_RELEASE(m_SaveRTV);
		SAFE_RELEASE(m_SaveSRV);
		SAFE_RELEASE(m_SaveUAV);
		SAFE_RELEASE(m_SaveMap);

		SAFE_RELEASE(m_pNumCB);
		SAFE_RELEASE(m_pCS);
		SAFE_RELEASE(m_NormalCS);

		SAFE_RELEASE(m_NoramlUAV);
		SAFE_RELEASE(m_NoramlMap);

		delete[]m_pBase;
	}

	void Init();
	void Dispatch();
public:
	ID3D11Buffer* m_pBuff_SineBase;
	ID3D11UnorderedAccessView* m_pUAV_SineBase;
	ID3D11ShaderResourceView* m_pSRV_SineBase;

	ID3D11Buffer* m_pNumCB;
	// Shaders, layouts and constants
	ID3D11ComputeShader* m_pCS;
	ID3D11ComputeShader* m_NormalCS;

	ID3D11Texture2D* m_SaveMap;		// 
	ID3D11ShaderResourceView* m_SaveSRV;
	ID3D11RenderTargetView* m_SaveRTV;
	ID3D11UnorderedAccessView* m_SaveUAV;

	ID3D11Texture2D* m_NoramlMap;		
	ID3D11UnorderedAccessView* m_NoramlUAV;

	TSineBaseNode*  m_pBase;
	int             m_iNum;
	int             m_iResolution;
};

