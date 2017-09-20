#include "stdafx.h"
#include <d3d11.h>
#include <D3Dcompiler.h>
#include <d3dx9.h>
#include "GpuFftPS.h"
extern ID3D11Device*           g_pd3dDevice;
extern ID3D11DeviceContext*    g_pImmediateContext;

//void createTextureAndViews(ID3D11Device* pd3dDevice, UINT width, UINT height, DXGI_FORMAT format,
	//ID3D11Texture2D** ppTex, ID3D11ShaderResourceView** ppSRV, ID3D11RenderTargetView** ppRTV);
HRESULT CompileShaderFromFile(WCHAR* szFileName, LPCSTR szEntryPoint, LPCSTR szShaderModel, ID3DBlob** ppBlobOut);
void createTextureAndViews(ID3D11Device* pd3dDevice, UINT width, UINT height, DXGI_FORMAT format,
	ID3D11Texture2D** ppTex, ID3D11ShaderResourceView** ppSRV, ID3D11RenderTargetView** ppRTV, const void *data = NULL,
	unsigned int SysMemPitch = 0, unsigned int SysMemSlicePitch = 0)
{
	// Create 2D texture
	D3D11_TEXTURE2D_DESC tex_desc;
	tex_desc.Width = width;
	tex_desc.Height = height;
	tex_desc.MipLevels = 1;
	tex_desc.ArraySize = 1;
	tex_desc.Format = format;
	tex_desc.SampleDesc.Count = 1;
	tex_desc.SampleDesc.Quality = 0;
	tex_desc.Usage = D3D11_USAGE_DEFAULT;// D3D11_USAGE_DEFAULT;
	tex_desc.BindFlags = ppRTV ? D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET : D3D11_BIND_SHADER_RESOURCE;
	tex_desc.CPUAccessFlags = 0;
	tex_desc.MiscFlags = 0;

	D3D11_SUBRESOURCE_DATA data_sur;
	data_sur.pSysMem = data;
	data_sur.SysMemPitch = SysMemPitch;
	data_sur.SysMemSlicePitch = 0;
	if (data)
	{
		pd3dDevice->CreateTexture2D(&tex_desc, &data_sur, ppTex);
		//pd3dDevice->CreateTexture2D(&tex_desc, NULL, ppTex);
	}
	else
	{
		pd3dDevice->CreateTexture2D(&tex_desc, NULL, ppTex);
	}
	
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

GpuFftPS::GpuFftPS(unsigned int width, unsigned int height, bool forward)
	: width_(width), height_(height), forward_(forward)
{
	//BOOST_ASSERT(0 == (width_ & (width_ - 1)));
	//BOOST_ASSERT(0 == (height_ & (height_ - 1)));

	log_x_ = static_cast<uint32_t>(log(static_cast<float>(width_)) / log(2.0f));
	log_y_ = static_cast<uint32_t>(log(static_cast<float>(height_)) / log(2.0f));

	lookup_i_wr_wi_x_tex_.resize(log_x_);
	lookup_i_wr_wi_y_tex_.resize(log_y_);

	std::vector<float> lookup_i_wr_wi_x(log_x_ * width_ * 4);
	std::vector<float> lookup_i_wr_wi_y(log_y_ * height_ * 4);

	this->CreateButterflyLookups(lookup_i_wr_wi_x, log_x_, width_);
	this->CreateButterflyLookups(lookup_i_wr_wi_y, log_y_, height_);

	//RenderFactory& rf = Context::Instance().RenderFactoryInstance();

	float* ptr = &lookup_i_wr_wi_x[0];
	for (uint32_t i = 0; i < log_x_; ++i)
	{
		
		createTextureAndViews(g_pd3dDevice, width_, 1, DXGI_FORMAT_R32G32B32A32_FLOAT,
			&lookup_i_wr_wi_x_tex_[i].LookUpMap, &lookup_i_wr_wi_x_tex_[i].LookUpSRV,
			NULL, ptr, width_ * sizeof(float) * 4, width_ * sizeof(float) * 4);
		
		ptr += width_ * 4;
	}

	ptr = &lookup_i_wr_wi_y[0];
	for (uint32_t i = 0; i < log_y_; ++i)
	{
		createTextureAndViews(g_pd3dDevice, 1, height_, DXGI_FORMAT_R32G32B32A32_FLOAT,
			&lookup_i_wr_wi_y_tex_[i].LookUpMap, &lookup_i_wr_wi_y_tex_[i].LookUpSRV,
			NULL, ptr, sizeof(float) * 4, sizeof(float) * 4 * height_);
		
		ptr += height_ * 4;
	}

	createTextureAndViews(g_pd3dDevice, width_, height_, DXGI_FORMAT_R32G32B32A32_FLOAT, &tmp_real_tex_[0].LookUpMap,
		&tmp_real_tex_[0].LookUpSRV, &tmp_real_tex_[0].LookUpRTV);
	createTextureAndViews(g_pd3dDevice, width_, height_, DXGI_FORMAT_R32G32B32A32_FLOAT, &tmp_real_tex_[1].LookUpMap,
		&tmp_real_tex_[1].LookUpSRV, &tmp_real_tex_[1].LookUpRTV);
	createTextureAndViews(g_pd3dDevice, width_, height_, DXGI_FORMAT_R32G32B32A32_FLOAT, &tmp_imag_tex_[0].LookUpMap,
		&tmp_imag_tex_[0].LookUpSRV, &tmp_imag_tex_[0].LookUpRTV);
	createTextureAndViews(g_pd3dDevice, width_, height_, DXGI_FORMAT_R32G32B32A32_FLOAT, &tmp_imag_tex_[1].LookUpMap,
		&tmp_imag_tex_[1].LookUpSRV, &tmp_imag_tex_[1].LookUpRTV);
	
	ID3DBlob* pBlobQuadVS = NULL;
	CompileShaderFromFile(L"./FFT.hlsl", "QuadVS", "vs_4_0", &pBlobQuadVS);
	g_pd3dDevice->CreateVertexShader(pBlobQuadVS->GetBufferPointer(), pBlobQuadVS->GetBufferSize(), NULL, &m_pQuadVS);
	
	ID3DBlob* pBlobfft_xPs1 = NULL;
	CompileShaderFromFile(L"./FFT.hlsl", "FFTXPS_1", "ps_4_0", &pBlobfft_xPs1);
	g_pd3dDevice->CreatePixelShader(pBlobfft_xPs1->GetBufferPointer(), pBlobfft_xPs1->GetBufferSize(), NULL, &fft_x_pp_1);
	pBlobfft_xPs1->Release();

	ID3DBlob* pBlobfft_yPs1 = NULL;
	CompileShaderFromFile(L"./FFT.hlsl", "FFTYPS_1", "ps_4_0", &pBlobfft_yPs1);
	g_pd3dDevice->CreatePixelShader(pBlobfft_yPs1->GetBufferPointer(), pBlobfft_yPs1->GetBufferSize(), NULL, &fft_y_pp_1);
	pBlobfft_yPs1->Release();

	ID3DBlob* pBlobfft_xPs = NULL;
	CompileShaderFromFile(L"./FFT.hlsl", "FFTXPS", "ps_4_0", &pBlobfft_xPs);
	g_pd3dDevice->CreatePixelShader(pBlobfft_xPs->GetBufferPointer(), pBlobfft_xPs->GetBufferSize(), NULL, &fft_x_pp_);
	pBlobfft_xPs->Release();

	ID3DBlob* pBlobfft_yPs = NULL;
	CompileShaderFromFile(L"./FFT.hlsl", "FFTYPS", "ps_4_0", &pBlobfft_yPs);
	g_pd3dDevice->CreatePixelShader(pBlobfft_yPs->GetBufferPointer(), pBlobfft_yPs->GetBufferSize(), NULL, &fft_y_pp_);
	pBlobfft_yPs->Release();

	D3D11_INPUT_ELEMENT_DESC quad_layout_desc[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};
	g_pd3dDevice->CreateInputLayout(quad_layout_desc, 1, pBlobQuadVS->GetBufferPointer(), pBlobQuadVS->GetBufferSize(), &m_pQuadLayout);
	//assert(m_pQuadLayout);
	pBlobQuadVS->Release();

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

	D3D11_SAMPLER_DESC sam_desc;
	sam_desc.Filter = D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT;
	sam_desc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
	sam_desc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
	sam_desc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
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

	
	// We use full sized data here. The value "output_width" should be actual_dim/2+1 though.
	UINT output_width = width;
	UINT output_height = height;
	UINT dtx_offset = output_width * output_height;
	UINT dty_offset = dtx_offset * 2;
	UINT immutable_consts[] = { output_width, output_height, dtx_offset, dty_offset };
	D3D11_SUBRESOURCE_DATA init_cb0 = { &immutable_consts[0], 0, 0 };

	D3D11_BUFFER_DESC cb_desc;
	cb_desc.Usage = D3D11_USAGE_IMMUTABLE;
	cb_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	cb_desc.CPUAccessFlags = 0;
	cb_desc.MiscFlags = 0;
	cb_desc.ByteWidth = PAD16(sizeof(immutable_consts));
	g_pd3dDevice->CreateBuffer(&cb_desc, &init_cb0, &m_pImmutableCB);
	//assert(m_pImmutableCB);

	float scale[] = { 1.0f / (width_*height_) };
	D3D11_SUBRESOURCE_DATA init_cb1 = { &scale[0], 0, 0 };
	cb_desc.Usage = D3D11_USAGE_DYNAMIC;
	cb_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	cb_desc.ByteWidth = PAD16(sizeof(scale));
	g_pd3dDevice->CreateBuffer(&cb_desc, &init_cb1, &m_pScaleCB);
	

	ID3D11Buffer* cbs[2] = { m_pImmutableCB , m_pScaleCB };
	g_pImmediateContext->PSSetConstantBuffers(0, 2, cbs);
}

void GpuFftPS::Execute(ID3D11RenderTargetView*  & out_real, ID3D11RenderTargetView*  & out_imag,
	ID3D11ShaderResourceView* &input)
{
	int active = 0;
	ID3D11RenderTargetView* old_target;
	ID3D11DepthStencilView* old_depth;
	g_pImmediateContext->OMGetRenderTargets(1, &old_target, &old_depth);
	float ClearColor[4] = { 0.0f,0.0f, 0.0f, 0.0f }; //red,green,blue,alpha

	D3D11_VIEWPORT old_viewport;
	UINT num_viewport = 1;
	g_pImmediateContext->RSGetViewports(&num_viewport, &old_viewport);

	D3D11_VIEWPORT new_vp = { 0, 0, (float)width_, (float)height_, 0.0f, 1.0f };
	g_pImmediateContext->RSSetViewports(1, &new_vp);
	for (uint32_t i = 0; i < log_x_; ++i)
	{
		g_pImmediateContext->ClearRenderTargetView(tmp_real_tex_[!active].LookUpRTV, ClearColor);
		g_pImmediateContext->ClearRenderTargetView(tmp_imag_tex_[!active].LookUpRTV, ClearColor);
		ID3D11RenderTargetView* rt_views[2] = { tmp_real_tex_[!active].LookUpRTV ,tmp_imag_tex_[!active].LookUpRTV };
		g_pImmediateContext->OMSetRenderTargets(2, rt_views, NULL);
		if (0 == i)
		{
			g_pImmediateContext->VSSetShader(m_pQuadVS, NULL, 0);
			g_pImmediateContext->PSSetShader(fft_x_pp_1, NULL, 0);
			ID3D11Buffer* cbs[1] = { m_pImmutableCB };
			g_pImmediateContext->PSSetConstantBuffers(0, 1, cbs);
			ID3D11ShaderResourceView* ps_srvs[2] = { input,lookup_i_wr_wi_x_tex_[i].LookUpSRV };
			g_pImmediateContext->PSSetShaderResources(0, 2, ps_srvs);
		
		}
		else
		{
			g_pImmediateContext->VSSetShader(m_pQuadVS, NULL, 0);
			g_pImmediateContext->PSSetShader(fft_x_pp_, NULL, 0);
			ID3D11Buffer* cbs[1] = { m_pImmutableCB };
			g_pImmediateContext->PSSetConstantBuffers(0, 1, cbs);
			ID3D11ShaderResourceView* ps_srvs[3] = {  lookup_i_wr_wi_x_tex_[i].LookUpSRV, tmp_real_tex_[active].LookUpSRV ,tmp_imag_tex_[active].LookUpSRV };
			g_pImmediateContext->PSSetShaderResources(1, 3, ps_srvs);
			
		}
		
		
	
		ID3D11Buffer* vbs[1] = { m_pQuadVB };
		UINT strides[1] = { sizeof(D3DXVECTOR4) };
		UINT offsets[1] = { 0 };
		g_pImmediateContext->IASetVertexBuffers(0, 1, &vbs[0], &strides[0], &offsets[0]);

		g_pImmediateContext->IASetInputLayout(m_pQuadLayout);
		g_pImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

		ID3D11SamplerState* samplers[1] = { m_pPointSamplerState };
		g_pImmediateContext->PSSetSamplers(0, 1, &samplers[0]);
		// Perform draw call
		g_pImmediateContext->Draw(4, 0);

		ID3D11ShaderResourceView*  ps_srvs[4] = {0,0,0,0};
		g_pImmediateContext->PSSetShaderResources(0, 4, ps_srvs);

		active = !active;
	}
	D3D11_MAPPED_SUBRESOURCE mapped_res;
	for (uint32_t i = 0; i < log_y_; ++i)
	{
		
		
		g_pImmediateContext->Map(m_pScaleCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped_res);
		float *f = (float*)mapped_res.pData;
		if (log_y_ - 1 == i)
		{
			if (forward_)
			{
				*f = -1.f;
			}
			else
			{
				*f = 1.0f / (width_ * height_);
			}
			g_pImmediateContext->ClearRenderTargetView(out_real, ClearColor);
			g_pImmediateContext->ClearRenderTargetView(out_imag, ClearColor);
			ID3D11RenderTargetView* rt_views[2] = { out_real ,out_imag };
			g_pImmediateContext->OMSetRenderTargets(2, rt_views, NULL);
		}
		else
		{
			*f = -1.f;
			g_pImmediateContext->ClearRenderTargetView(tmp_real_tex_[!active].LookUpRTV, ClearColor);
			g_pImmediateContext->ClearRenderTargetView(tmp_imag_tex_[!active].LookUpRTV, ClearColor);
			ID3D11RenderTargetView* rt_views[2] = { tmp_real_tex_[!active].LookUpRTV ,tmp_imag_tex_[!active].LookUpRTV };
			g_pImmediateContext->OMSetRenderTargets(2, rt_views, NULL);
			//fft_y_pp_->OutputPin(0, tmp_real_tex_[!active]);
			//fft_y_pp_->OutputPin(1, tmp_imag_tex_[!active]);
		}
		g_pImmediateContext->Unmap(m_pScaleCB, 0);

		ID3D11Buffer* cbs[2] = { m_pImmutableCB, m_pScaleCB };
		g_pImmediateContext->PSSetConstantBuffers(0, 2, cbs);

		ID3D11ShaderResourceView* ps_srvs[3] = { lookup_i_wr_wi_y_tex_[i].LookUpSRV,
			tmp_real_tex_[active].LookUpSRV ,tmp_imag_tex_[active].LookUpSRV };
		g_pImmediateContext->PSSetShaderResources(1, 3, ps_srvs);
		g_pImmediateContext->VSSetShader(m_pQuadVS, NULL, 0);
		g_pImmediateContext->PSSetShader(fft_y_pp_, NULL, 0);
		ID3D11Buffer* vbs[1] = { m_pQuadVB };
		UINT strides[1] = { sizeof(D3DXVECTOR4) };
		UINT offsets[1] = { 0 };
		g_pImmediateContext->IASetVertexBuffers(0, 1, &vbs[0], &strides[0], &offsets[0]);

		g_pImmediateContext->IASetInputLayout(m_pQuadLayout);
		g_pImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

		ID3D11SamplerState* samplers[1] = { m_pPointSamplerState };
		g_pImmediateContext->PSSetSamplers(0, 1, &samplers[0]);
		// Perform draw call
		g_pImmediateContext->Draw(4, 0);
		ps_srvs[0] = { 0 };
		ps_srvs[1] = { 0 };
		ps_srvs[2] = { 0 };
		g_pImmediateContext->PSSetShaderResources(1, 3, ps_srvs);
		active = !active;
	}


	// Pop RT
	g_pImmediateContext->RSSetViewports(1, &old_viewport);
	g_pImmediateContext->OMSetRenderTargets(1, &old_target, old_depth);

	if (old_target)
		old_target->Release();
	if (old_depth)
		old_depth->Release();
	//SAFE_RELEASE(old_target);
	//SAFE_RELEASE(old_depth);
}

int GpuFftPS::BitReverse(int i, int n)
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

void GpuFftPS::ComputeWeight(float& wr, float& wi, int n, int k)
{
	float phase = 2 * D3DX_PI * k / n;
	wi = sinf(phase);
	wr = cosf(phase);
	//MathLib::sincos(phase, wi, wr);
	wi = forward_ ? -wi : wi;
}

void GpuFftPS::CreateButterflyLookups(std::vector<float>& lookup_i_wr_wi, int log_n, int n)
{
	float* ptr = &lookup_i_wr_wi[0];

	for (int i = 0; i < log_n; ++i)
	{
		int const blocks = 1UL << (log_n - 1 - i);
		int const hinputs = 1UL << i;
		for (int j = 0; j < blocks; ++j)
		{
			for (int k = 0; k < hinputs; ++k)
			{
				int i1, i2, j1, j2;
				i1 = j * hinputs * 2 + k;
				i2 = j * hinputs * 2 + hinputs + k;
				if (0 == i)
				{
					j1 = this->BitReverse(i1, n);
					j2 = this->BitReverse(i2, n);
				}
				else
				{
					j1 = i1;
					j2 = i2;
				}

				float wr, wi;
				this->ComputeWeight(wr, wi, n, k * blocks);

				ptr[i1 * 4 + 0] = float((j1 + 0.5f) / n);
				ptr[i1 * 4 + 1] = float((j2 + 0.5f) / n);
				ptr[i1 * 4 + 2] = float(+wr);
				ptr[i1 * 4 + 3] = float(+wi);

				ptr[i2 * 4 + 0] = float((j1 + 0.5f) / n);
				ptr[i2 * 4 + 1] = float((j2 + 0.5f) / n);
				ptr[i2 * 4 + 2] = float(-wr);
				ptr[i2 * 4 + 3] = float(-wi);
			}
		}

		ptr += n * 4;
	}
}

GpuFftPS::~GpuFftPS()
{
	for( auto it = lookup_i_wr_wi_x_tex_.begin(); it != lookup_i_wr_wi_x_tex_.end();
		++it )
	{
		SAFE_RELEASE(it->LookUpSRV);
		SAFE_RELEASE(it->LookUpMap);
	}

	for (auto it = lookup_i_wr_wi_y_tex_.begin(); it != lookup_i_wr_wi_y_tex_.end();
		++it)
	{
		SAFE_RELEASE(it->LookUpSRV);
		SAFE_RELEASE(it->LookUpMap);
	}

	for (RenderTargetView &temp : tmp_real_tex_)
	{
		SAFE_RELEASE(temp.LookUpRTV);
		SAFE_RELEASE(temp.LookUpSRV);
		SAFE_RELEASE(temp.LookUpMap);
	}

	for (RenderTargetView &temp : tmp_imag_tex_)
	{
		SAFE_RELEASE(temp.LookUpRTV);
		SAFE_RELEASE(temp.LookUpSRV);
		SAFE_RELEASE(temp.LookUpMap);
	}


	SAFE_RELEASE(m_pQuadVS);
	SAFE_RELEASE(m_pImmutableCB);
	SAFE_RELEASE(fft_x_pp_1);
	SAFE_RELEASE(fft_y_pp_1);
	SAFE_RELEASE(fft_x_pp_);
	SAFE_RELEASE(fft_y_pp_);

	SAFE_RELEASE(m_pPointSamplerState);

	SAFE_RELEASE(m_pQuadLayout);
	SAFE_RELEASE(m_pQuadVB);

	SAFE_RELEASE(m_pScaleCB);
}