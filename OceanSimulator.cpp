#include "stdafx.h"
#include <d3d11.h>
#include "OceanSimulator.h"
#include "GpuFftPS.h"
//#include "DXUT.h"
//#include "SDKmisc.h"
#include <D3Dcompiler.h>
#include <math.h>
#include "EncodeColor.h"
#include <D3DX11tex.h>
#include <fstream>

extern ID3D11Device*           g_pd3dDevice;
extern ID3D11DeviceContext*    g_pImmediateContext;
float Gauss()
{
	float u1 = rand() / (float)RAND_MAX;
	float u2 = rand() / (float)RAND_MAX;
	if (u1 < 1e-6f)
		u1 = 1e-6f;
	return sqrtf(-2 * logf(u1)) * cosf(2 * D3DX_PI * u2);
}

// Phillips Spectrum
// K: normalized wave vector, W: wind direction, v: wind velocity, a: amplitude constant
float Phillips(D3DXVECTOR2 K, D3DXVECTOR2 W, float v, float a, float dir_depend)
{
	// largest possible wave from constant wind of velocity v
	float l = v * v / GRAV_ACCEL;
	// damp out waves with very small length w << l
	float w = l / 1000;

	float Ksqr = K.x * K.x + K.y * K.y;
	float Kcos = K.x * W.x + K.y * W.y;
	float phillips = a * expf(-1 / (l * l * Ksqr)) / (Ksqr * Ksqr * Ksqr) * (Kcos * Kcos);

	// filter out waves moving opposite to wind
	if (Kcos < 0)
		phillips *= dir_depend;

	// damp out waves with very small length w << l
	return phillips * expf(-Ksqr * w * w);
}

void createBufferAndUAV(ID3D11Device* pd3dDevice, void* data, UINT byte_width, UINT byte_stride,
	ID3D11Buffer** ppBuffer, ID3D11UnorderedAccessView** ppUAV, ID3D11ShaderResourceView** ppSRV)
{
	// Create buffer
	D3D11_BUFFER_DESC buf_desc;
	buf_desc.ByteWidth = byte_width;
	buf_desc.Usage = D3D11_USAGE_DEFAULT;
	buf_desc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
	buf_desc.CPUAccessFlags = 0;
	buf_desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	buf_desc.StructureByteStride = byte_stride;

	D3D11_SUBRESOURCE_DATA init_data = { data, 0, 0 };

	pd3dDevice->CreateBuffer(&buf_desc, data != NULL ? &init_data : NULL, ppBuffer);
	//assert(*ppBuffer);

	// Create undordered access view
	D3D11_UNORDERED_ACCESS_VIEW_DESC uav_desc;
	uav_desc.Format = DXGI_FORMAT_UNKNOWN;
	uav_desc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
	uav_desc.Buffer.FirstElement = 0;
	uav_desc.Buffer.NumElements = byte_width / byte_stride;
	uav_desc.Buffer.Flags = 0;

	pd3dDevice->CreateUnorderedAccessView(*ppBuffer, &uav_desc, ppUAV);
	//assert(*ppUAV);

	// Create shader resource view
	D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc;
	srv_desc.Format = DXGI_FORMAT_UNKNOWN;
	srv_desc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
	srv_desc.Buffer.FirstElement = 0;
	srv_desc.Buffer.NumElements = byte_width / byte_stride;

	pd3dDevice->CreateShaderResourceView(*ppBuffer, &srv_desc, ppSRV);
	//assert(*ppSRV);
}

void createTextureAndViews(ID3D11Device* pd3dDevice, UINT width, UINT height, DXGI_FORMAT format,
	ID3D11Texture2D** ppTex, ID3D11ShaderResourceView** ppSRV, ID3D11RenderTargetView** ppRTV)
{
	// Create 2D texture
	D3D11_TEXTURE2D_DESC tex_desc;
	tex_desc.Width = width;
	tex_desc.Height = height;
	tex_desc.MipLevels = 0;
	tex_desc.ArraySize = 1;
	tex_desc.Format = format;
	tex_desc.SampleDesc.Count = 1;
	tex_desc.SampleDesc.Quality = 0;
	tex_desc.Usage = D3D11_USAGE_DEFAULT;
	tex_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
	tex_desc.CPUAccessFlags = 0;
	tex_desc.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;

	pd3dDevice->CreateTexture2D(&tex_desc, NULL, ppTex);
	//assert(*ppTex);

	// Create shader resource view
	(*ppTex)->GetDesc(&tex_desc);
	if (ppSRV)
	{
		D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc;
		srv_desc.Format = format;
		srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		srv_desc.Texture2D.MipLevels = tex_desc.MipLevels;
		srv_desc.Texture2D.MostDetailedMip = 0;

		pd3dDevice->CreateShaderResourceView(*ppTex, &srv_desc, ppSRV);
		//assert(*ppSRV);
	}

	// Create render target view
	if (ppRTV)
	{
		D3D11_RENDER_TARGET_VIEW_DESC rtv_desc;
		rtv_desc.Format = format;
		rtv_desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
		rtv_desc.Texture2D.MipSlice = 0;

		pd3dDevice->CreateRenderTargetView(*ppTex, &rtv_desc, ppRTV);
		//assert(*ppRTV);
	}
}

HRESULT CompileShaderFromFile(WCHAR* szFileName, LPCSTR szEntryPoint, LPCSTR szShaderModel, ID3DBlob** ppBlobOut)
{
	HRESULT hr = S_OK;

	// find the file
	//WCHAR str[MAX_PATH];
	//V_RETURN(DXUTFindDXSDKMediaFileCch(str, MAX_PATH, szFileName));

	// open the file
	HANDLE hFile = CreateFile(szFileName, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
		FILE_FLAG_SEQUENTIAL_SCAN, NULL);
	if (INVALID_HANDLE_VALUE == hFile)
		return E_FAIL;

	// Get the file size
	LARGE_INTEGER FileSize;
	GetFileSizeEx(hFile, &FileSize);

	// create enough space for the file data
	BYTE* pFileData = new BYTE[FileSize.LowPart];
	if (!pFileData)
		return E_OUTOFMEMORY;

	// read the data in
	DWORD BytesRead;
	if (!ReadFile(hFile, pFileData, FileSize.LowPart, &BytesRead, NULL))
		return E_FAIL;

	CloseHandle(hFile);

	// Compile the shader
	char pFilePathName[MAX_PATH];
	WideCharToMultiByte(CP_ACP, 0, szFileName, -1, pFilePathName, MAX_PATH, NULL, NULL);
	ID3DBlob* pErrorBlob;
	hr = D3DCompile(pFileData, FileSize.LowPart, pFilePathName, NULL, NULL, szEntryPoint, szShaderModel, D3D10_SHADER_ENABLE_STRICTNESS, 0, ppBlobOut, &pErrorBlob);

	delete[]pFileData;

	if (FAILED(hr))
	{
		OutputDebugStringA((char*)pErrorBlob->GetBufferPointer());
		pErrorBlob->Release();
		pErrorBlob = nullptr;
		//SAFE_RELEASE(pErrorBlob);
		return hr;
	}

	if (pErrorBlob)
	{
		pErrorBlob->Release();
		pErrorBlob = nullptr;
	}
	return S_OK;
}

void COceanSimulator::CalulerCycleAndOmega(float &Length, unsigned int &Dimension, float &time_scale, CCycleOmega &CycleOmega)
{
	D3DXVECTOR2 KMin(D3DX_PI * 2 / Length, 0);// D3DX_PI * 2 / Length);
	D3DXVECTOR2 KMax(D3DX_PI * Dimension / Length, D3DX_PI * Dimension / Length);
	CycleOmega.fMaxOmega = sqrt(GRAV_ACCEL * D3DXVec2Length(&KMax));
	CycleOmega.fMinOmega = sqrt(GRAV_ACCEL * D3DXVec2Length(&KMin));
	CycleOmega.fMaxCycle = D3DX_PI * 2 / CycleOmega.fMinOmega;// *time_scale;
	CycleOmega.fMinCycle = D3DX_PI * 2 / CycleOmega.fMaxOmega;
}

float COceanSimulator::ComputeCycleAndOmega()
{

	//m_tBakeInfo.fMinOmega = D3DX_PI * 2 / m_tBakeInfo.fCycleTime;
	float minCycle = 1 / m_tBakeInfo.FPS;
	D3DXVECTOR2 KMin(D3DX_PI * 2 / m_tBakeInfo.fLength, 0);
	m_tBakeInfo.fMinOmega = sqrt(GRAV_ACCEL * D3DXVec2Length(&KMin));
	m_tBakeInfo.fCycleTime = D3DX_PI * 2 / m_tBakeInfo.fMinOmega;
	float CycleNum = m_tBakeInfo.fCycleTime / (minCycle);
	float second;
	float fraction;
	fraction = modff(CycleNum, &second);
	if (fraction > 0.00001f)
		second += 1.f;
	m_tBakeInfo.fMinOmega = D3DX_PI * 2 / (second * minCycle);
	m_tBakeInfo.fCycleTime = D3DX_PI * 2 / m_tBakeInfo.fMinOmega;
	m_tBakeInfo.bakeNum = second - 1;
	//m_tBakeInfo.fMinOmega = D3DX_PI * 2 / m_tBakeInfo.fCycleTime;
	D3DXVECTOR2 KMax(D3DX_PI * m_tBakeInfo.iDimension / m_tBakeInfo.fLength, D3DX_PI * m_tBakeInfo.iDimension / m_tBakeInfo.fLength);
	float fMaxOmega = sqrt(GRAV_ACCEL * D3DXVec2Length(&KMax));
	return m_tBakeInfo.fMinOmega;
}

void COceanSimulator::Init()
{
	int height_map_size = m_tBakeInfo.iDimension * m_tBakeInfo.iDimension;
	D3DXVECTOR2* h0_data = new D3DXVECTOR2[height_map_size];
	float* omega_data = new float[height_map_size ];
	InitHeightMap(h0_data, omega_data);

	char* zero_data = new char[3 * height_map_size * sizeof(float) * 2];
	memset(zero_data, 0, 3 * height_map_size * sizeof(float) * 2);

	UINT float2_stride = 2 * sizeof(float);
	createBufferAndUAV(g_pd3dDevice, h0_data, height_map_size * float2_stride, float2_stride, &m_pBuffer_Float2_H0, &m_pUAV_H0, &m_pSRV_H0);

	createBufferAndUAV(g_pd3dDevice, omega_data, height_map_size * sizeof(float), sizeof(float), &m_pBuffer_Float_Omega, &m_pUAV_Omega, &m_pSRV_Omega);

	// Put H(t), Dx(t) and Dy(t) into one buffer because CS4.0 allows only 1 UAV at a time
	createBufferAndUAV(g_pd3dDevice, zero_data, 3 * height_map_size * float2_stride, float2_stride, &m_pBuffer_Float2_Ht, &m_pUAV_Ht, &m_pSRV_Ht);
	
	// Constant buffers
	UINT actual_dim = m_tBakeInfo.iDimension;
	UINT input_width = actual_dim;
	// We use full sized data here. The value "output_width" should be actual_dim/2+1 though.
	UINT output_width = actual_dim;
	UINT output_height = actual_dim;
	UINT dtx_offset = actual_dim * actual_dim;
	UINT dty_offset = actual_dim * actual_dim * 2;
	UINT immutable_consts[] = { actual_dim, input_width, output_width, output_height, dtx_offset, dty_offset };
	D3D11_SUBRESOURCE_DATA init_cb0 = { &immutable_consts[0], 0, 0 };

	D3D11_BUFFER_DESC cb_desc;
	cb_desc.Usage = D3D11_USAGE_IMMUTABLE;
	cb_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	cb_desc.CPUAccessFlags = 0;
	cb_desc.MiscFlags = 0;
	cb_desc.ByteWidth = PAD16(sizeof(immutable_consts));
	g_pd3dDevice->CreateBuffer(&cb_desc, &init_cb0, &m_pImmutableCB);
	//assert(m_pImmutableCB);

	ID3D11Buffer* cbs[1] = { m_pImmutableCB };
	g_pImmediateContext->CSSetConstantBuffers(0, 1, cbs);
	g_pImmediateContext->PSSetConstantBuffers(0, 1, cbs);

	cb_desc.Usage = D3D11_USAGE_DYNAMIC;
	cb_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	cb_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	cb_desc.MiscFlags = 0;
	cb_desc.ByteWidth = PAD16(sizeof(float) * 3);
	g_pd3dDevice->CreateBuffer(&cb_desc, NULL, &m_pPerFrameCB);
	//assert(m_pPerFrameCB);
	
	// Compute shaders
	ID3DBlob* pBlobUpdateSpectrumCS = NULL;

	CompileShaderFromFile(L"./ocean_simulator_cs.hlsl", "UpdateSpectrumCS", "cs_4_0", &pBlobUpdateSpectrumCS);
	//assert(pBlobUpdateSpectrumCS);

	g_pd3dDevice->CreateComputeShader(pBlobUpdateSpectrumCS->GetBufferPointer(), pBlobUpdateSpectrumCS->GetBufferSize(), NULL, &m_pUpdateSpectrumCS);
	//assert(m_pUpdateSpectrumCS);

	pBlobUpdateSpectrumCS->Release();
	pBlobUpdateSpectrumCS = NULL;

	createTextureAndViews(g_pd3dDevice, m_tBakeInfo.iDimension, m_tBakeInfo.iDimension, DXGI_FORMAT_R32G32B32A32_FLOAT, &InverRealMap,
		&InverRealSRV, &InverRealRTV);

	createTextureAndViews(g_pd3dDevice, m_tBakeInfo.iDimension, m_tBakeInfo.iDimension, DXGI_FORMAT_R32G32B32A32_FLOAT, &InverImagMap,
		&InverImagSRV, &InverImagRTV);

	createTextureAndViews(g_pd3dDevice, m_tBakeInfo.iDimension, m_tBakeInfo.iDimension, DXGI_FORMAT_R32G32B32A32_FLOAT, &m_pDisplacementMap,
		&m_pDisplacementSRV, &m_pDisplacementRTV);
	createTextureAndViews(g_pd3dDevice, m_tBakeInfo.iDimension, m_tBakeInfo.iDimension, DXGI_FORMAT_R32G32B32A32_FLOAT, &m_pGradientMap, &m_pGradientSRV, &m_pGradientRTV);
	createTextureAndViews(g_pd3dDevice, m_tBakeInfo.iDimension, m_tBakeInfo.iDimension, DXGI_FORMAT_R8G8B8A8_UNORM, &m_pNormalMap, &m_pNormalSRV, &m_pNormalRTV);

	// Samplers
	D3D11_SAMPLER_DESC sam_desc;
	sam_desc.Filter = D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT;
	sam_desc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	sam_desc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	sam_desc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	sam_desc.MipLODBias = 0;
	sam_desc.MaxAnisotropy = 1;
	sam_desc.ComparisonFunc = D3D11_COMPARISON_NEVER;
	sam_desc.BorderColor[0] = 1.0f;
	sam_desc.BorderColor[1] = 1.0f;
	sam_desc.BorderColor[2] = 1.0f;
	sam_desc.BorderColor[3] = 1.0f;
	sam_desc.MinLOD = -FLT_MAX;
	sam_desc.MaxLOD = FLT_MAX;
	g_pd3dDevice->CreateSamplerState(&sam_desc, &m_pPointSamplerState);

	D3D11_BUFFER_DESC vb_desc;
	vb_desc.ByteWidth = 4 * sizeof(D3DXVECTOR4);
	vb_desc.Usage = D3D11_USAGE_IMMUTABLE;
	vb_desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	vb_desc.CPUAccessFlags = 0;
	vb_desc.MiscFlags = 0;

	float quad_verts[] =
	{
		-1, -1, 0, 1,
		-1,  1, 0, 1,
		1, -1, 0, 1,
		1,  1, 0, 1,
	};
	D3D11_SUBRESOURCE_DATA init_data;
	init_data.pSysMem = &quad_verts[0];
	init_data.SysMemPitch = 0;
	init_data.SysMemSlicePitch = 0;

	g_pd3dDevice->CreateBuffer(&vb_desc, &init_data, &m_pQuadVB);

	ID3DBlob* pBlobQuadVS = NULL;
	CompileShaderFromFile(L"./ocean_simulator_vs_ps.hlsl", "QuadVS", "vs_4_0", &pBlobQuadVS);
	g_pd3dDevice->CreateVertexShader(pBlobQuadVS->GetBufferPointer(), pBlobQuadVS->GetBufferSize(), NULL, &m_pQuadVS);

	ID3DBlob* pBlobUpdateDisplacementPS = NULL;
	ID3DBlob* pBlobGenGradientFoldingPS = NULL;
	CompileShaderFromFile(L"./ocean_simulator_vs_ps.hlsl", "UpdateDisplacementPS", "ps_4_0", &pBlobUpdateDisplacementPS);
	CompileShaderFromFile(L"./ocean_simulator_vs_ps.hlsl", "GenGradientFoldingPS", "ps_4_0", &pBlobGenGradientFoldingPS);
	g_pd3dDevice->CreatePixelShader(pBlobUpdateDisplacementPS->GetBufferPointer(), pBlobUpdateDisplacementPS->GetBufferSize(), NULL, &m_pUpdateDisplacementPS);
	g_pd3dDevice->CreatePixelShader(pBlobGenGradientFoldingPS->GetBufferPointer(), pBlobGenGradientFoldingPS->GetBufferSize(), NULL, &m_pGenGradientFoldingPS);
	pBlobUpdateDisplacementPS->Release();
	pBlobGenGradientFoldingPS->Release();

	D3D11_INPUT_ELEMENT_DESC quad_layout_desc[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};
	g_pd3dDevice->CreateInputLayout(quad_layout_desc, 1, pBlobQuadVS->GetBufferPointer(), pBlobQuadVS->GetBufferSize(), &m_pQuadLayout);
	//assert(m_pQuadLayout);
	pBlobQuadVS->Release();

	D3D11_TEXTURE2D_DESC tex_desc;
	tex_desc.Width = m_tBakeInfo.iDimension;
	tex_desc.Height = m_tBakeInfo.iDimension;
	tex_desc.MipLevels = 0;
	tex_desc.ArraySize = 1;
	tex_desc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	tex_desc.SampleDesc.Count = 1;
	tex_desc.SampleDesc.Quality = 0;
	tex_desc.Usage = D3D11_USAGE_STAGING;
	tex_desc.BindFlags = 0;
	tex_desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
	tex_desc.MiscFlags = 0;

	g_pd3dDevice->CreateTexture2D(&tex_desc, NULL, &ReadDisplacementMap);

	//D3D11_TEXTURE2D_DESC tex_desc;
	tex_desc.Width = m_tBakeInfo.iDimension;
	tex_desc.Height = m_tBakeInfo.iDimension;
	tex_desc.MipLevels = 0;// log(m_tBakeInfo.iDimension) / log(2);
	tex_desc.ArraySize = 1;
	tex_desc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	tex_desc.SampleDesc.Count = 1;
	tex_desc.SampleDesc.Quality = 0;
	tex_desc.Usage = D3D11_USAGE_STAGING;
	tex_desc.BindFlags = 0 ;
	tex_desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
	tex_desc.MiscFlags = 0;

	g_pd3dDevice->CreateTexture2D(&tex_desc, NULL, &ReadGradientMap);

	tex_desc.Width = m_tBakeInfo.iDimension;
	tex_desc.Height = m_tBakeInfo.iDimension;
	tex_desc.MipLevels = 1;// log(m_tBakeInfo.iDimension) / log(2);
	tex_desc.ArraySize = 1;
	tex_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	tex_desc.SampleDesc.Count = 1;
	tex_desc.SampleDesc.Quality = 0;
	tex_desc.Usage = D3D11_USAGE_DYNAMIC;
	tex_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	tex_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	tex_desc.MiscFlags = 0;

	HRESULT  hr = g_pd3dDevice->CreateTexture2D(&tex_desc, NULL, &SaveToFileDisplacementMap);

	g_pd3dDevice->CreateTexture2D(&tex_desc, NULL, &SaveToFileGradientMap);

	fft = new GpuFftPS(m_tBakeInfo.iDimension, m_tBakeInfo.iDimension, false);
	delete[] omega_data;
	delete[] h0_data;
	ClearSave();

	saveDisplacement.resize(m_tBakeInfo.bakeNum);
	for (float *&i : saveDisplacement)
	{
		i = new float[m_tBakeInfo.iDimension*m_tBakeInfo.iDimension * 4];
	}
	saveGradien.resize(m_tBakeInfo.bakeNum);
	for (float *&i : saveGradien)
	{
		i = new float[m_tBakeInfo.iDimension*m_tBakeInfo.iDimension * 4];
	}

	DisplacementImage;
	GradienImage;
	GradienErrorImage;
	DisplacementImage.resize(m_tBakeInfo.bakeNum);
	for (unsigned char *&i : DisplacementImage)
	{
		i = new unsigned char[m_tBakeInfo.iDimension*m_tBakeInfo.iDimension * 4];
	}
	GradienImage.resize(m_tBakeInfo.bakeNum);
	for (unsigned char *&i : GradienImage)
	{
		i = new unsigned char[m_tBakeInfo.iDimension*m_tBakeInfo.iDimension * 4];
	}
	GradienErrorImage.resize(m_tBakeInfo.bakeNum);
	for (unsigned char *&i : GradienErrorImage)
	{
		i = new unsigned char[m_tBakeInfo.iDimension*m_tBakeInfo.iDimension * 4];
	}
	cutframe = 0;
	cuttime = 0;
}

void COceanSimulator::InitHeightMap(D3DXVECTOR2* out_h0, float* out_omega)
{
	int i, j;
	D3DXVECTOR2 K, Kn;

	D3DXVECTOR2 wind_dir;
	D3DXVec2Normalize(&wind_dir, &m_tBakeInfo.wind_dir);
	float a = m_tBakeInfo.wave_amplitude * 1e-7f;	// It is too small. We must scale it for editing.
	float v = m_tBakeInfo.wind_speed;
	float dir_depend = m_tBakeInfo.wind_dependency;

	int height_map_dim = m_tBakeInfo.iDimension;
	float patch_length = m_tBakeInfo.fLength;

	// initialize random generator.
	srand(0);

	for (i = 0; i < height_map_dim; i++)
	{
		// K is wave-vector, range [-|DX/W, |DX/W], [-|DY/H, |DY/H]
		K.y = (-height_map_dim / 2.0f + i) * (2 * D3DX_PI / patch_length);

		for (j = 0; j < height_map_dim; j++)
		{
			
			K.x = (-height_map_dim / 2.0f + j) * (2 * D3DX_PI / patch_length);
			float temp;
			temp = out_omega[i * (height_map_dim)+j] = m_tBakeInfo.fMinOmega * static_cast<int>(sqrtf(GRAV_ACCEL * sqrtf(K.x * K.x + K.y * K.y)) / m_tBakeInfo.fMinOmega);
			//temp = out_omega[i * (height_map_dim)+j] = sqrtf(GRAV_ACCEL * sqrtf(K.x * K.x + K.y * K.y));
			bool discard = out_omega[i * (height_map_dim)+j] == 0
				|| out_omega[i * (height_map_dim)+j] > m_tBakeInfo.fMaxOmega;
			//;
			float phil = (K.x == 0 && K.y == 0) 
				|| discard 
				?
				0 : sqrtf(Phillips(K, wind_dir, v, a, dir_depend));

			out_h0[i * (height_map_dim ) + j].x = float(phil * Gauss() * HALF_SQRT_2);
			out_h0[i * (height_map_dim ) + j].y = float(phil * Gauss() * HALF_SQRT_2);
		}
	}
}


void COceanSimulator::updateDisplacementMap(float time)
{
	time = 1.f / m_tBakeInfo.FPS;
	cuttime += time;
	// Compute shader
	g_pImmediateContext->CSSetShader(m_pUpdateSpectrumCS, NULL, 0);

	// Buffers
	ID3D11ShaderResourceView* cs0_srvs[2] = { m_pSRV_H0, m_pSRV_Omega };
	g_pImmediateContext->CSSetShaderResources(0, 2, cs0_srvs);

	ID3D11UnorderedAccessView* cs0_uavs[1] = { m_pUAV_Ht };
	g_pImmediateContext->CSSetUnorderedAccessViews(0, 1, cs0_uavs, (UINT*)(&cs0_uavs[0]));

	// Consts
	D3D11_MAPPED_SUBRESOURCE mapped_res;
	g_pImmediateContext->Map(m_pPerFrameCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped_res);
	//assert(mapped_res.pData);
	float* per_frame_data = (float*)mapped_res.pData;
	// g_Time
	per_frame_data[0] = cuttime;
	// g_ChoppyScale
	per_frame_data[1] = m_tBakeInfo.choppy_scale;
	// g_GridLen
	per_frame_data[2] = m_tBakeInfo.iDimension / m_tBakeInfo.fLength;
	g_pImmediateContext->Unmap(m_pPerFrameCB, 0);

	ID3D11Buffer* cs_cbs[2] = { m_pImmutableCB, m_pPerFrameCB };
	g_pImmediateContext->CSSetConstantBuffers(0, 2, cs_cbs);

	// Run the CS
	UINT group_count_x = (m_tBakeInfo.iDimension + BLOCK_SIZE_X - 1) / BLOCK_SIZE_X;
	UINT group_count_y = (m_tBakeInfo.iDimension + BLOCK_SIZE_Y - 1) / BLOCK_SIZE_Y;
	g_pImmediateContext->Dispatch(group_count_x, group_count_y, 1);

	// Unbind resources for CS
	cs0_uavs[0] = NULL;
	g_pImmediateContext->CSSetUnorderedAccessViews(0, 1, cs0_uavs, (UINT*)(&cs0_uavs[0]));
	cs0_srvs[0] = NULL;
	cs0_srvs[1] = NULL;
	g_pImmediateContext->CSSetShaderResources(0, 2, cs0_srvs);

	//GpuFftPS  fft(m_tBakeInfo.iDimension, m_tBakeInfo.iDimension, false);
	fft->Execute(InverRealRTV, InverImagRTV, m_pSRV_Ht);


	ID3D11RenderTargetView* old_target;
	ID3D11DepthStencilView* old_depth;
	g_pImmediateContext->OMGetRenderTargets(1, &old_target, &old_depth);
	D3D11_VIEWPORT old_viewport;
	UINT num_viewport = 1;
	g_pImmediateContext->RSGetViewports(&num_viewport, &old_viewport);

	D3D11_VIEWPORT new_vp = { 0, 0, (float)m_tBakeInfo.iDimension, (float)m_tBakeInfo.iDimension, 0.0f, 1.0f };
	g_pImmediateContext->RSSetViewports(1, &new_vp);

	ID3D11RenderTargetView* rt_views[1] = { m_pDisplacementRTV };
	g_pImmediateContext->OMSetRenderTargets(1, rt_views, NULL);

	g_pImmediateContext->VSSetShader(m_pQuadVS, NULL, 0);
	g_pImmediateContext->PSSetShader(m_pUpdateDisplacementPS, NULL, 0);

	// Constants
	ID3D11Buffer* ps_cbs[2] = { m_pImmutableCB, m_pPerFrameCB };
	g_pImmediateContext->PSSetConstantBuffers(0, 2, ps_cbs);

	ID3D11ShaderResourceView* ps_srvs[1] = { InverRealSRV };
	g_pImmediateContext->PSSetShaderResources(1, 1, ps_srvs);

	g_pImmediateContext->PSSetSamplers(1, 0, &m_pPointSamplerState);

	// IA setup
	ID3D11Buffer* vbs[1] = { m_pQuadVB };
	UINT strides[1] = { sizeof(D3DXVECTOR4) };
	UINT offsets[1] = { 0 };
	g_pImmediateContext->IASetVertexBuffers(0, 1, &vbs[0], &strides[0], &offsets[0]);

	g_pImmediateContext->IASetInputLayout(m_pQuadLayout);
	g_pImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	// Perform draw call
	g_pImmediateContext->Draw(4, 0);

	// Unbind
	ps_srvs[0] = NULL;
	g_pImmediateContext->PSSetShaderResources(0, 1, ps_srvs);


	// ----------------------------------- Generate Normal ----------------------------------------
	// Set RT
	ID3D11RenderTargetView* rt_Normalviews[2] = { m_pGradientRTV , m_pNormalRTV };
	//rt_views[0] = m_pGradientRTV;
	g_pImmediateContext->OMSetRenderTargets(2, rt_Normalviews, NULL);

	// VS & PS
	g_pImmediateContext->VSSetShader(m_pQuadVS, NULL, 0);
	g_pImmediateContext->PSSetShader(m_pGenGradientFoldingPS, NULL, 0);

	// Texture resource and sampler
	ps_srvs[0] = m_pDisplacementSRV;
	g_pImmediateContext->PSSetShaderResources(0, 1, ps_srvs);

	ID3D11SamplerState* samplers[1] = { m_pPointSamplerState };
	g_pImmediateContext->PSSetSamplers(0, 1, &samplers[0]);

	// Perform draw call
	g_pImmediateContext->Draw(4, 0);

	// Unbind
	ps_srvs[0] = NULL;
	g_pImmediateContext->PSSetShaderResources(0, 1, ps_srvs);

	// Pop RT
	g_pImmediateContext->RSSetViewports(1, &old_viewport);
	g_pImmediateContext->OMSetRenderTargets(1, &old_target, old_depth);
	SAFE_RELEASE(old_target);
	SAFE_RELEASE(old_depth);

	//g_pImmediateContext->GenerateMips(m_pGradientSRV);
	if (cutframe < m_tBakeInfo.bakeNum)
	{
		g_pImmediateContext->CopyResource(ReadDisplacementMap, m_pDisplacementMap);
		g_pImmediateContext->CopyResource(ReadGradientMap, m_pGradientMap);
		//D3D11CalcSubresource

		D3D11_MAPPED_SUBRESOURCE DisplacementMapinfo = { 0 };
		g_pImmediateContext->Map(ReadDisplacementMap, 0, D3D11_MAP_READ, 0, &DisplacementMapinfo);
		D3D11_MAPPED_SUBRESOURCE GradientMapinfo = { 0 };
		HRESULT  hr = g_pImmediateContext->Map(ReadGradientMap, 0, D3D11_MAP_READ, 0, &GradientMapinfo);

		std::vector<unsigned char*> DisplacementImage;
		DisplacementImage.resize(m_tBakeInfo.iDimension*m_tBakeInfo.iDimension);

		if (DisplacementMapinfo.pData && GradientMapinfo.pData && cutframe < saveDisplacement.size())
		{
			float *pDisplacement = (float *)DisplacementMapinfo.pData;
			float *pGradient = (float *)GradientMapinfo.pData;

			memcpy(saveDisplacement[cutframe], DisplacementMapinfo.pData, m_tBakeInfo.iDimension * m_tBakeInfo.iDimension *
				4 * sizeof(float) );
			memcpy(saveGradien[cutframe], GradientMapinfo.pData, m_tBakeInfo.iDimension * m_tBakeInfo.iDimension *
				4 * sizeof(float) );

		}
		g_pImmediateContext->Unmap(ReadDisplacementMap, 0);
		g_pImmediateContext->Unmap(ReadGradientMap, 0);
		WCHAR filrname[128] = { 0 };
		wsprintf(filrname, TEXT("./bakefile/%s_NormalMap_%d.PNG"), name,cutframe);
		hr = D3DX11SaveTextureToFile(g_pImmediateContext, m_pNormalMap, D3DX11_IFF_PNG, filrname);
		int ff = 0;
		++ff;
	}
	
	if (cutframe == m_tBakeInfo.bakeNum)
	{
		int index = 0;
		WCHAR filename[128] = { 0 };
		float scalelog;
		float MinValue;
		EncodeImge(saveDisplacement, DisplacementImage, scalelog, MinValue);
		std::wofstream Displacementfile;
		wsprintf(filename, TEXT("./bakefile/%s_DisplacementInfo.txt"), name);
		Displacementfile.open(filename, std::ios::out);
		Displacementfile << "MinValue:" << MinValue << "\n" << "scalelog:" << scalelog << "\n";
		Displacementfile << "time" << 1.0f / m_tBakeInfo.FPS;
		Displacementfile.close();
		for (unsigned char *&i : DisplacementImage)
		{
			D3D11_MAPPED_SUBRESOURCE saveinfo = { 0 };
			g_pImmediateContext->Map(SaveToFileDisplacementMap, 0, D3D11_MAP_WRITE_DISCARD, 0, &saveinfo);
			if (saveinfo.pData)
			{
				memcpy(saveinfo.pData, i, m_tBakeInfo.iDimension * m_tBakeInfo.iDimension *
					4);
			}

			g_pImmediateContext->Unmap(SaveToFileDisplacementMap, 0);
			wsprintf(filename, TEXT("./bakefile/%s_DisplacementMap_%d.PNG"), name, index++);
			HRESULT  hr = D3DX11SaveTextureToFile(g_pImmediateContext, SaveToFileDisplacementMap, D3DX11_IFF_PNG, filename);

			int ff = 0;
			++ff;
		}
		index = 0;
		EncodeImge(saveGradien, GradienImage/*, GradienErrorImage*/, scalelog, MinValue);
		wsprintf(filename, TEXT("./bakefile/%s_GradientInfo.txt"), name);
		Displacementfile.open(filename, std::ios::out);
		Displacementfile << "MinValue:" << MinValue << "\n" << "scalelog:" << scalelog;
		Displacementfile.close();
		for (unsigned char *&i : GradienImage)
		{
			D3D11_MAPPED_SUBRESOURCE saveinfo = { 0 };
			g_pImmediateContext->Map(SaveToFileGradientMap, 0, D3D11_MAP_WRITE_DISCARD, 0, &saveinfo);
			if (saveinfo.pData)
			{
				memcpy(saveinfo.pData, i, m_tBakeInfo.iDimension * m_tBakeInfo.iDimension *
					4);
			}

			g_pImmediateContext->Unmap(SaveToFileGradientMap, 0);
			wsprintf(filename, TEXT("./bakefile/%s_GradientMap_%d.PNG"), name, index++);
			HRESULT  hr = D3DX11SaveTextureToFile(g_pImmediateContext, SaveToFileGradientMap, D3DX11_IFF_PNG, filename);

			int ff = 0;
			++ff;
		}
		index = 0;
		for (unsigned char *&i : GradienErrorImage)
		{
			D3D11_MAPPED_SUBRESOURCE saveinfo = { 0 };
			g_pImmediateContext->Map(SaveToFileGradientMap, 0, D3D11_MAP_WRITE_DISCARD, 0, &saveinfo);
			if (saveinfo.pData)
			{
				memcpy(saveinfo.pData, i, m_tBakeInfo.iDimension * m_tBakeInfo.iDimension *
					4);
			}

			g_pImmediateContext->Unmap(SaveToFileGradientMap, 0);
			wsprintf(filename, TEXT("./bakefile/%s_GradientErrorMap_%d.PNG"), name, index++);
			HRESULT  hr = D3DX11SaveTextureToFile(g_pImmediateContext, SaveToFileGradientMap, D3DX11_IFF_PNG, filename);

			int ff = 0;
			++ff;
		}
	}
	//ReadDisplacementMap ReadGradientMap
	++cutframe;
	
	
	float scalelog;
	MaxDxyz[0] = 112.f;
	MinDxyz[0] = -140.5f;
	MinDxyz[0] -= 1;
	float color[4] = {-140, -140 , -140 };
	float color1[4];
	std::vector<unsigned char> arr;
	arr.resize(4);
	EncodeColor::ComputeCoefficient(MinDxyz[0], MaxDxyz[0], scalelog);
	EncodeColor::Encode(&arr[0], color, 4, MinDxyz[0], MaxDxyz[0], scalelog);
	EncodeColor::GetDxyz(arr[3], arr[0], arr[1], arr[2], MinDxyz[0], scalelog, color1);
}

void COceanSimulator::EncodeImge(std::vector<float*> &saveImge, std::vector<unsigned char*> &EncodeImgefloat, float &LogScale, float &MinValue)
{
	float MaxValue = -10000.f;
	MinValue = 3.40282346638528860e+38;

	for (float *&p : saveImge)
	{
		for(int i = 0; i < m_tBakeInfo.iDimension * m_tBakeInfo.iDimension * 4; i += 4)
		{
			MaxValue = max(MaxValue, p[i]);
			MaxValue = max(MaxValue, p[i + 1]);
			MaxValue = max(MaxValue, p[i + 2]);

			MinValue = min(MinValue, p[i]);
			MinValue = min(MinValue, p[i + 1] );
			MinValue = min(MinValue, p[i + 2] );
		}
	}
	MinValue -= 1;
	EncodeColor::ComputeCoefficient(MinValue, MaxValue, LogScale);

	for (int i = 0; i < saveImge.size(); ++i)
	{
		EncodeColor::Encode(EncodeImgefloat[i], saveImge[i], m_tBakeInfo.iDimension * m_tBakeInfo.iDimension * 4,
			MinValue, MaxValue, LogScale);
	}
}

void COceanSimulator::EncodeImge(std::vector<float*> &saveImge, std::vector<unsigned char*> &EncodeImgefloat, std::vector<unsigned char*> &ErrorImgefloat,  float &LogScale, float &MinValue)
{
	float MaxValue = -10000.f;
	MinValue = 3.40282346638528860e+38;

	for (float *&p : saveImge)
	{
		for (int i = 0; i < m_tBakeInfo.iDimension * m_tBakeInfo.iDimension * 4; i += 4)
		{
			MaxValue = max(MaxValue, p[i]);
			MaxValue = max(MaxValue, p[i + 1]);
			MaxValue = max(MaxValue, p[i + 2]);

			MinValue = min(MinValue, p[i]);
			MinValue = min(MinValue, p[i + 1]);
			MinValue = min(MinValue, p[i + 2]);
		}
	}
	MinValue -= 1;
	EncodeColor::ComputeCoefficient(MinValue, MaxValue, LogScale);

	for (int i = 0; i < saveImge.size(); ++i)
	{
		EncodeColor::Encode(EncodeImgefloat[i], ErrorImgefloat[i],saveImge[i], m_tBakeInfo.iDimension * m_tBakeInfo.iDimension * 4,
			MinValue, MaxValue, LogScale);
	}
}