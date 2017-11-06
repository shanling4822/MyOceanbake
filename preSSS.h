#pragma once
#include <d3dx9math.h>
#include <vector>



class PreSSS
{
public:
	PreSSS();
	~PreSSS()
	{
		SAFE_RELEASE(m_SaveRTV);
		SAFE_RELEASE(m_SaveSRV);
		SAFE_RELEASE(m_SaveUAV);
		SAFE_RELEASE(m_SaveMap);

		SAFE_RELEASE(m_pCS);

	}

	void Init();
	void Dispatch();
public:

	ID3D11ComputeShader* m_pCS;

	ID3D11Texture2D* m_SaveMap;		
	ID3D11ShaderResourceView* m_SaveSRV;
	ID3D11RenderTargetView* m_SaveRTV;
	ID3D11UnorderedAccessView* m_SaveUAV;

};

