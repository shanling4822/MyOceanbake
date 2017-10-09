#include "stdafx.h"
#include <d3d11.h>
#include "SinBaseNoise.h"
#include <D3DX11tex.h>
#include <d3dx9math.h>
#include <math.h>
#include "nvtt.h"
extern ID3D11Device*           g_pd3dDevice;
extern ID3D11DeviceContext*    g_pImmediateContext;
void createBufferAndUAV(ID3D11Device* pd3dDevice, void* data, UINT byte_width, UINT byte_stride,
	ID3D11Buffer** ppBuffer, ID3D11UnorderedAccessView** ppUAV, ID3D11ShaderResourceView** ppSRV);
void createTextureAndViews(ID3D11Device* pd3dDevice, UINT width, UINT height, DXGI_FORMAT format,
	ID3D11Texture2D** ppTex, ID3D11ShaderResourceView** ppSRV, ID3D11RenderTargetView** ppRTV);
HRESULT CompileShaderFromFile(WCHAR* szFileName, LPCSTR szEntryPoint, LPCSTR szShaderModel, ID3DBlob** ppBlobOut);

int BitReverse(int i, int n)
{
	int j = i;

	int m = n;
	int sum = 0;
	int w = 1;
	m /= 2;
	while (m != 0)
	{
		j = (i & m) > m - 1;
		sum += j * w;
		w *= 2;
		m /= 2;
	}
	return sum;
}
D3DXVECTOR2 Hammersley(unsigned int  Index, unsigned int NumSamples, unsigned int Random1, unsigned int Random2)
{
	float temp;
	float E1 = modff((float)Index / NumSamples + float(Random1 & 0xffff) / (1 << 16), &temp);
	float E2 = float(BitReverse(Index,32) ^ Random2) * 2.3283064365386963e-10;
	return D3DXVECTOR2(E1, E2);
}
SinBaseNoise::SinBaseNoise()
{
	unsigned int random1 = rand();// % 1024;
	unsigned int random2 = rand();// % 1024;
	m_iResolution = 1024;
	m_iNum = 100;
	m_pBase = new TSineBaseNode[m_iNum];
	for (int i = 0; i < m_iNum; ++i)
	{
		D3DXVECTOR2 v = Hammersley(i, m_iNum, random1, random2);
		TSineBaseNode *p = &m_pBase[i];
		p->x = rand() % 1024;// (float)rand() / RAND_MAX *1024.f;//v.x *m_iResolution ;
		p->y = rand() % 1024;// (float)rand() / RAND_MAX *1024.f;//v.y *m_iResolution ;
		p->wave = (float)rand() / RAND_MAX;
	}
}



void SinBaseNoise::Init()
{
	createBufferAndUAV(g_pd3dDevice, m_pBase, sizeof(TSineBaseNode) * m_iNum, sizeof(TSineBaseNode), &m_pBuff_SineBase, &m_pUAV_SineBase, &m_pSRV_SineBase);
	//createTextureAndViews(g_pd3dDevice, m_iResolution, m_iResolution, DXGI_FORMAT_R8G8B8A8_UNORM, &m_SaveMap,
		//&m_SaveSRV, &m_SaveRTV);
	D3D11_TEXTURE2D_DESC SineTexDesc;
	SineTexDesc.Width = m_iResolution;
	SineTexDesc.Height = m_iResolution;
	SineTexDesc.MipLevels = 1;
	SineTexDesc.ArraySize = 1;
	SineTexDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	SineTexDesc.SampleDesc.Count = 1;
	SineTexDesc.SampleDesc.Quality = 0;
	SineTexDesc.Usage = D3D11_USAGE_DEFAULT;
	SineTexDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
	SineTexDesc.CPUAccessFlags = 0;
	SineTexDesc.MiscFlags = 0;

	g_pd3dDevice->CreateTexture2D(&SineTexDesc, 0, &m_SaveMap);

	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
	srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = 1;

	g_pd3dDevice->CreateShaderResourceView(m_SaveMap, &srvDesc, &m_SaveSRV);

	D3D11_UNORDERED_ACCESS_VIEW_DESC uav_desc;
	uav_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	uav_desc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
	uav_desc.Texture2D.MipSlice = 0;

	g_pd3dDevice->CreateUnorderedAccessView(m_SaveMap, &uav_desc, &m_SaveUAV);

	D3D11_TEXTURE2D_DESC NormalTexDesc;
	NormalTexDesc.Width = m_iResolution;
	NormalTexDesc.Height = m_iResolution;
	NormalTexDesc.MipLevels = 1;
	NormalTexDesc.ArraySize = 1;
	NormalTexDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	NormalTexDesc.SampleDesc.Count = 1;
	NormalTexDesc.SampleDesc.Quality = 0;
	NormalTexDesc.Usage = D3D11_USAGE_DEFAULT;
	NormalTexDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
	NormalTexDesc.CPUAccessFlags = 0;
	NormalTexDesc.MiscFlags = 0;

	g_pd3dDevice->CreateTexture2D(&NormalTexDesc, 0, &m_NoramlMap);

	g_pd3dDevice->CreateUnorderedAccessView(m_NoramlMap, &uav_desc, &m_NoramlUAV);

	// Compute shaders
	ID3DBlob* pBlobUpdateSpectrumCS = NULL;

	CompileShaderFromFile(L"./sinbasenoise_cs.hlsl", "SinbasenoiseCS", "cs_5_0", &pBlobUpdateSpectrumCS);
	//assert(pBlobUpdateSpectrumCS);

	g_pd3dDevice->CreateComputeShader(pBlobUpdateSpectrumCS->GetBufferPointer(), pBlobUpdateSpectrumCS->GetBufferSize(), NULL, &m_pCS);
	//assert(m_pUpdateSpectrumCS);

	pBlobUpdateSpectrumCS->Release();
	pBlobUpdateSpectrumCS = NULL;

	CompileShaderFromFile(L"./normal_cs.hlsl", "NormalCS", "cs_5_0", &pBlobUpdateSpectrumCS);
	g_pd3dDevice->CreateComputeShader(pBlobUpdateSpectrumCS->GetBufferPointer(), pBlobUpdateSpectrumCS->GetBufferSize(), NULL, &m_NormalCS);
	pBlobUpdateSpectrumCS->Release();
	pBlobUpdateSpectrumCS = NULL;

	UINT immutable_consts[] = { m_iNum };
	D3D11_SUBRESOURCE_DATA init_cb0 = { &immutable_consts[0], 0, 0 };

	D3D11_BUFFER_DESC cb_desc;
	cb_desc.Usage = D3D11_USAGE_IMMUTABLE;
	cb_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	cb_desc.CPUAccessFlags = 0;
	cb_desc.MiscFlags = 0;
	cb_desc.ByteWidth = PAD16(sizeof(int));
	g_pd3dDevice->CreateBuffer(&cb_desc, &init_cb0, &m_pNumCB);

	ID3D11Buffer* cbs[1] = { m_pNumCB };
	g_pImmediateContext->CSSetConstantBuffers(0, 1, cbs);
}
#define BLOCK_SIZE_X 16
#define BLOCK_SIZE_Y 16
void SinBaseNoise::Dispatch()
{
	g_pImmediateContext->CSSetShader(m_pCS, NULL, 0);

	// Buffers
	ID3D11ShaderResourceView* cs0_srvs[1] = { m_pSRV_SineBase };
	g_pImmediateContext->CSSetShaderResources(0, 1, cs0_srvs);

	ID3D11UnorderedAccessView* cs0_uavs[1] = { m_SaveUAV };
	g_pImmediateContext->CSSetUnorderedAccessViews(0, 1, cs0_uavs, (UINT*)(&cs0_uavs[0]));

	// Consts
	// Run the CS
	UINT group_count_x = (m_iResolution + BLOCK_SIZE_X - 1) / BLOCK_SIZE_X;
	UINT group_count_y = (m_iResolution + BLOCK_SIZE_Y - 1) / BLOCK_SIZE_Y;
	g_pImmediateContext->Dispatch(group_count_x, group_count_y, 1);

	// Unbind resources for CS
	cs0_uavs[0] = NULL;
	g_pImmediateContext->CSSetUnorderedAccessViews(0, 1, cs0_uavs, (UINT*)(&cs0_uavs[0]));
	cs0_srvs[0] = NULL;
	//cs0_srvs[1] = NULL;
	g_pImmediateContext->CSSetShaderResources(0, 1, cs0_srvs);

	g_pImmediateContext->CSSetShader(m_NormalCS, NULL, 0);
	cs0_srvs[0] = { m_SaveSRV };
	g_pImmediateContext->CSSetShaderResources(1, 1, cs0_srvs);

	cs0_uavs[0] = { m_NoramlUAV };
	g_pImmediateContext->CSSetUnorderedAccessViews(1, 1, cs0_uavs, (UINT*)(&cs0_uavs[0]));
	g_pImmediateContext->Dispatch(group_count_x, group_count_y, 1);

	// Unbind resources for CS
	//cs0_uavs[0] = NULL;
	//g_pImmediateContext->CSSetUnorderedAccessViews(0, 1, cs0_uavs, (UINT*)(&cs0_uavs[0]));
	//cs0_srvs[0] = NULL;
	//g_pImmediateContext->CSSetShaderResources(0, 1, cs0_srvs);

	//WCHAR filename[128] = { 0 };
	//wsprintf(filename, TEXT("./bakefile/%s_GradientErrorMap_%d.PNG"), name, index++);
	HRESULT  hr = D3DX11SaveTextureToFile(g_pImmediateContext, m_SaveMap, D3DX11_IFF_PNG, TEXT("./bakefile/sine.PNG"));
	//hr = D3DX11SaveTextureToFile(g_pImmediateContext, m_NoramlMap, D3DX11_IFF_PNG, TEXT("./bakefile/normal.PNG"));

	nvtt::Surface      img;
	img.load("./bakefile/sine.PNG");
	img.toGreyScale(1, 0, 0, 0);
	img.toNormalMap(1.0f / 3.0f, 1.0f / 3.0f, 1.0f / 3.0f, 0);
	img.packNormals();
	img.save("./bakefile/normal.TGA");
}