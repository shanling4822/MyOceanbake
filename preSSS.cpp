#include "stdafx.h"
#include <d3d11.h>
#include "preSSS.h"
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

#define TableResolution 512

PreSSS::PreSSS()
{
}



void PreSSS::Init()
{
	
	D3D11_TEXTURE2D_DESC TexDesc;
	TexDesc.Width = TableResolution;
	TexDesc.Height = TableResolution;
	TexDesc.MipLevels = 1;
	TexDesc.ArraySize = 1;
	TexDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	TexDesc.SampleDesc.Count = 1;
	TexDesc.SampleDesc.Quality = 0;
	TexDesc.Usage = D3D11_USAGE_DEFAULT;
	TexDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
	TexDesc.CPUAccessFlags = 0;
	TexDesc.MiscFlags = 0;

	g_pd3dDevice->CreateTexture2D(&TexDesc, 0, &m_SaveMap);

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


	// Compute shaders
	ID3DBlob* pBlobUpdateSpectrumCS = NULL;

	CompileShaderFromFile(L"./PreSSS_cs.hlsl", "PreSSSCS", "cs_5_0", &pBlobUpdateSpectrumCS);
	//assert(pBlobUpdateSpectrumCS);

	g_pd3dDevice->CreateComputeShader(pBlobUpdateSpectrumCS->GetBufferPointer(), pBlobUpdateSpectrumCS->GetBufferSize(), NULL, &m_pCS);
	//assert(m_pUpdateSpectrumCS);

	pBlobUpdateSpectrumCS->Release();
	pBlobUpdateSpectrumCS = NULL;

}
#define BLOCK_SIZE_X 16
#define BLOCK_SIZE_Y 16
void PreSSS::Dispatch()
{
	g_pImmediateContext->CSSetShader(m_pCS, NULL, 0);

	ID3D11UnorderedAccessView* cs0_uavs[1] = { m_SaveUAV };
	g_pImmediateContext->CSSetUnorderedAccessViews(0, 1, cs0_uavs, (UINT*)(&cs0_uavs[0]));

	// Consts
	// Run the CS
	UINT group_count_x = (TableResolution + BLOCK_SIZE_X - 1) / BLOCK_SIZE_X;
	UINT group_count_y = (TableResolution + BLOCK_SIZE_Y - 1) / BLOCK_SIZE_Y;
	g_pImmediateContext->Dispatch(group_count_x, group_count_y, 1);

	// Unbind resources for CS
	cs0_uavs[0] = NULL;
	g_pImmediateContext->CSSetUnorderedAccessViews(0, 1, cs0_uavs, (UINT*)(&cs0_uavs[0]));


	HRESULT  hr = D3DX11SaveTextureToFile(g_pImmediateContext, m_SaveMap, D3DX11_IFF_PNG, TEXT("./bakefile/ssstable.PNG"));

}