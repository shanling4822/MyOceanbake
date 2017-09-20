#pragma once
#include <vector>

struct LookUpView
{
	ID3D11Texture2D* LookUpMap;		// (RGBA32F)
	ID3D11ShaderResourceView* LookUpSRV;
	//ID3D11RenderTargetView* LookUpRTV;
};

struct RenderTargetView
{
	ID3D11Texture2D* LookUpMap;		// (RGBA32F)
	ID3D11ShaderResourceView* LookUpSRV;
	ID3D11RenderTargetView* LookUpRTV;
};
class GpuFftPS
{
public:
	GpuFftPS(unsigned int width, unsigned int height, bool forward);
	~GpuFftPS();

	void Execute(ID3D11RenderTargetView*  & out_real, ID3D11RenderTargetView* & out_imag,
		ID3D11ShaderResourceView* &input);

private:
	int BitReverse(int i, int n);
	void ComputeWeight(float& wr, float& wi, int n, int k);
	void CreateButterflyLookups(std::vector<float>& lookup_i_wr_wi, int log_n, int n);

private:
	unsigned int width_, height_;
	bool forward_;

	unsigned int log_x_, log_y_;

	std::vector<LookUpView> lookup_i_wr_wi_x_tex_;
	std::vector<LookUpView> lookup_i_wr_wi_y_tex_;

	RenderTargetView tmp_real_tex_[2];
	RenderTargetView tmp_imag_tex_[2];

	ID3D11VertexShader* m_pQuadVS;
	ID3D11Buffer* m_pImmutableCB;
	ID3D11PixelShader* fft_x_pp_1;
	ID3D11PixelShader* fft_y_pp_1;
	ID3D11PixelShader* fft_x_pp_;
	ID3D11PixelShader* fft_y_pp_;

	ID3D11SamplerState* m_pPointSamplerState;

	ID3D11InputLayout* m_pQuadLayout;
	ID3D11Buffer* m_pQuadVB;

	ID3D11Buffer* m_pScaleCB;
};