#include "stdafx.h"
#include "3DTP.h"
#include "InputManager.h"
#include "D3Dcompiler.h"
#include "Camera.h"

#include "ImGui/imgui.h"
#include "ImGui/imgui_impl_dx11.h"
#include "DirectXTK\DDSTextureLoader.h"

// Global Variables:
HINSTANCE					hInst;	// current instance
HWND						hWnd;	// windows handle used in DirectX initialization
IAEngine::InputManager*		g_pInputManager = NULL;
IDXGISwapChain*				g_pSwapChain = NULL;
ID3D11Device*				g_pDevice = NULL;
ID3D11DeviceContext*		g_pImmediateContext = NULL;
ID3D11RenderTargetView*		g_pRenderTargetView = NULL;

ID3D11RenderTargetView*     g_pRenderTargetViewCopy = NULL;
ID3D11ShaderResourceView*   g_pShaderResourceViewCopy = NULL;

ID3D11Texture2D*			g_pDepthStencil = NULL;
ID3D11DepthStencilView*		g_pDepthStencilView = NULL;
ID3D11Buffer*				g_pConstantBuffer = NULL; // CB (added after given code)

ID3D11Texture2D*			g_pRender = NULL;

unsigned short m_sizeX;
unsigned short m_sizeY;
float *m_height;
float m_maxZ = 50.0;

// Forward declarations
bool	CreateWindows(HINSTANCE, int, HWND& hWnd);
bool	CreateDevice();
bool	CreateDefaultRT();
bool	CreateCopyRT();
bool	CompileShader(LPCWSTR pFileName, bool bPixel, LPCSTR pEntrypoint, ID3DBlob** ppCompiledShader);//utiliser un L devant une chaine de caractère pour avoir un wchar* comme L"MonEffet.fx"
bool LoadRAW(const std::string& map);

/*
Exemple of possible triangle coodinates in 3D
P0 0.0f, 1.0f, 0.0f,
P1 5.0f, 1.0f, 0.0f,
P2 5.0f, 1.0f, 5.0f,
*/
using namespace DirectX::SimpleMath;
using namespace DirectX;

struct VertexInput {
	float x, y, z;
	float u, v;
};

struct MonCB
{
	//float WorldViewProj[4][4]
	Matrix WorldViewProj;
};

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	MSG oMsg;
	ulong	iElaspedTime = 0;
	ulong	iLastTime = 0;

	hInst = hInstance;
	if (!CreateWindows (hInstance, nCmdShow, hWnd))
	{
		MessageBox(NULL, L"Erreur lors de la création de la fenêtre", L"Error", 0);
		return false;
	}
	g_pInputManager = new IAEngine::InputManager();
	if (!g_pInputManager->Create(hInst, hWnd))
	{
		MessageBox(NULL, L"Erreur lors de la création de l'input manager", L"Error", 0);
		delete g_pInputManager;
		return false;
	}
	if (!CreateDevice())
	{
		MessageBox(NULL, L"Erreur lors de la création du device DirectX 11", L"Error", 0);
		return false;
	}
	if (!CreateDefaultRT())
	{
		MessageBox(NULL, L"Erreur lors de la création des render targets", L"Error", 0);
		return false;
	}
	if (!CreateCopyRT())
	{
		MessageBox(NULL, L"Erreur lors de la création des render targets de copy", L"Error", 0);
		return false;
	}

	ID3D11RasterizerState* pRasterizerState;
	D3D11_RASTERIZER_DESC oDesc;
	ZeroMemory(&oDesc, sizeof(D3D11_RASTERIZER_DESC));
	oDesc.FillMode = D3D11_FILL_SOLID;
	oDesc.CullMode = D3D11_CULL_NONE;
	g_pDevice->CreateRasterizerState(&oDesc, &pRasterizerState);
	g_pImmediateContext->RSSetState(pRasterizerState);

	D3D11_VIEWPORT vp;
	vp.Width = WINDOW_WIDTH;
	vp.Height = WINDOW_HEIGHT;
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;
	vp.TopLeftX = 0;
	vp.TopLeftY = 0;

	//Create and fill other DirectX Stuffs like Vertex/Index buffer, shaders
	LPCWSTR shaderPath = L"Shaders/MonShader.fx";
	ID3DBlob* pVSBlob = NULL;
	ID3DBlob* pPSBlob = NULL;
	CompileShader(shaderPath, false, "DiffuseVS", &pVSBlob);
	CompileShader(shaderPath, true, "DiffusePS", &pPSBlob);

	ID3D11VertexShader* pVertexShader;
	ID3D11PixelShader* pPixelShader;
	HRESULT hrCVS = g_pDevice->CreateVertexShader(pVSBlob->GetBufferPointer(),
		pVSBlob->GetBufferSize(), NULL, &pVertexShader);
	HRESULT hrCPS = g_pDevice->CreatePixelShader(pPSBlob->GetBufferPointer(),
		pPSBlob->GetBufferSize(), NULL, &pPixelShader);

	D3D11_INPUT_ELEMENT_DESC layoutPosColor[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};

	ID3D11InputLayout* pVertexLayout;
	HRESULT hrCIL = g_pDevice->CreateInputLayout(
		layoutPosColor, 2,
		pVSBlob->GetBufferPointer(),
		pVSBlob->GetBufferSize(), &pVertexLayout);

	g_pImmediateContext->IASetInputLayout(pVertexLayout);
	g_pImmediateContext->VSSetShader(pVertexShader, NULL, 0);
	g_pImmediateContext->PSSetShader(pPixelShader, NULL, 0);
	//END SHADERS

	//BEGIN TEXTURE
	ID3D11Resource* pTexture;
	ID3D11ShaderResourceView* pTextureView;
	HRESULT hrDDS = CreateDDSTextureFromFile(g_pDevice, L"Resources/terraintexture.dds", &pTexture, &pTextureView, 0, NULL);

	D3D11_SAMPLER_DESC samplerDesc;
	samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	samplerDesc.MipLODBias = 0.0f;
	samplerDesc.MaxAnisotropy = 1;
	samplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
	samplerDesc.MinLOD = -FLT_MAX;
	samplerDesc.MaxLOD = FLT_MAX;
	
	ID3D11SamplerState* myLinearWrapSampler;
	HRESULT hrSS = g_pDevice->CreateSamplerState(&samplerDesc, &myLinearWrapSampler);
	g_pImmediateContext->PSSetSamplers(0, 1, &myLinearWrapSampler);

	ID3D11Resource* pTextureDetail;
	ID3D11ShaderResourceView* pTextureViewDetail;
	HRESULT hrDDS2 = CreateDDSTextureFromFile(g_pDevice, L"Resources/detail.dds", &pTextureDetail, &pTextureViewDetail, 0, NULL);
	
	ID3D11ShaderResourceView* pTextureViews[] = { pTextureView , pTextureViewDetail };

	g_pImmediateContext->PSSetShaderResources(0, 2, pTextureViews);
	//END TEXTURE

	//VERTEX BUFFER
	/*VertexInput vertexs[] = {
		{ 0.0f, 0.5f, 0.0f, 1.0f, 0.0f, 0.0f },
		{ 0.5f, -0.5f, 0.0f, 0.0f, 1.0f, 0.0f },
		{ -0.5f, -0.5f, 0.0f, 0.0f, 0.0f, 1.0f }
	};*/

	/*VertexInput vertexs[] = {
		{ 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f },
		{ 5.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f },
		{ 5.0f, 1.0f, 5.0f, 0.0f, 0.0f, 1.0f }
	};*/
	LoadRAW("Resources/terrainheight.raw");

	const unsigned short TailleX = m_sizeX, TailleY = m_sizeY;
	VertexInput *vertexs = new VertexInput[TailleX * TailleY];
	for (int i = 0; i < TailleX; i++)
	{
		for (int j = 0; j < TailleY; j++) {
			float height = m_height[i * TailleY + j];

			/*if (height > 100)
				vertexs[i * TailleY + j] = { (float)i, (float)j, height, 1.0f, 0.0f, 0.0f };
			else if (height > 50)
				vertexs[i * TailleY + j] = { (float)i, (float)j, height, 0.0f, 1.0f, 0.0f };
			else
				vertexs[i * TailleY + j] = { (float)i, (float)j, height, 0.0f, 0.0f, 1.0f };*/

			vertexs[i * TailleY + j] = { (float)i, (float)j, height, ((float) i / TailleX), (1.0f - (float) j / TailleY) };
		}
	}

	ID3D11Buffer* pVB = NULL;
	D3D11_BUFFER_DESC vbDesc;
	vbDesc.Usage = D3D11_USAGE_IMMUTABLE;
	vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	vbDesc.ByteWidth = sizeof(VertexInput) * TailleX * TailleY;
	vbDesc.MiscFlags = 0;
	vbDesc.StructureByteStride = 0;
	vbDesc.CPUAccessFlags = 0;

	D3D11_SUBRESOURCE_DATA resData;
	resData.pSysMem = vertexs;
	resData.SysMemPitch = 0;
	resData.SysMemSlicePitch = 0;
	g_pDevice->CreateBuffer(&vbDesc, &resData, &pVB);

	// select which vertex buffer to display
	UINT stride = sizeof(VertexInput);
	UINT offset = 0;
	g_pImmediateContext->IASetVertexBuffers(0, 1, &pVB, &stride, &offset);
	g_pImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	//END VERTEX BUFFER

	//INDEX BUFFER
	ID3D11Buffer *pIndexBuffer = NULL;

	unsigned int *indices = new unsigned int[6 * (TailleX - 1) * (TailleY - 1)];
	// de la forme i, i + a, i + b, i + c, i + a, i + b. A vous de déterminez les valeur de a (le
	// point d’après), b(le point une ligne après et c(le point après celui une ligne après).
	int index = 0;
	for (int i = 0; i < (TailleX - 1); i++)
	{
		for (int j = 0; j < (TailleY - 1); j++) {		
			indices[index] = i * TailleY + j;
			indices[index + 1] = (i * TailleY + j) + 1;
			indices[index + 2] = ((i + 1) * TailleY + j);

			indices[index + 3] = ((i + 1) * TailleY + j) + 1;
			indices[index + 4] = ((i + 1) * TailleY + j);
			indices[index + 5] = (i * TailleY + j) + 1;

			index += 6;
		}
	}

	// Define the resource data.
	D3D11_SUBRESOURCE_DATA InitData;
	InitData.pSysMem = indices;
	InitData.SysMemPitch = 0;
	InitData.SysMemSlicePitch = 0;

	// Fill in a buffer description.
	D3D11_BUFFER_DESC ibDesc;
	ibDesc.Usage = D3D11_USAGE_IMMUTABLE;
	ibDesc.ByteWidth = sizeof(unsigned int) * (6 * (TailleX - 1) * (TailleY - 1));
	ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
	ibDesc.CPUAccessFlags = 0;
	ibDesc.MiscFlags = 0;
	ibDesc.StructureByteStride = 0;

	// Create the buffer with the device.
	HRESULT hr = g_pDevice->CreateBuffer(&ibDesc, &InitData, &pIndexBuffer);
	if (FAILED(hr))
		return hr;

	// Set the buffer.
	g_pImmediateContext->IASetIndexBuffer(pIndexBuffer, DXGI_FORMAT_R32_UINT, 0);
	//END INDEX  BUFFER

	//CONSTANT BUFFER
	D3D11_BUFFER_DESC cbDesc;
	ZeroMemory(&cbDesc, sizeof(cbDesc));
	cbDesc.ByteWidth = sizeof(MonCB);
	cbDesc.Usage = D3D11_USAGE_DEFAULT;
	cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

	g_pDevice->CreateBuffer(&cbDesc, NULL, &g_pConstantBuffer);
	g_pImmediateContext->VSSetConstantBuffers(0, 1, &g_pConstantBuffer);
	//END CONSTANT BUFFER

	IAEngine::FreeCamera oFreeCamera;
	iLastTime = timeGetTime();
	PeekMessage( &oMsg, NULL, 0, 0, PM_NOREMOVE );
	while ( oMsg.message != WM_QUIT )
	{
		if (PeekMessage( &oMsg, NULL, 0, 0, PM_REMOVE )) 
		{
			TranslateMessage( &oMsg );
			DispatchMessage( &oMsg );
		}
		else
		{
			ulong iTime = timeGetTime();
			iElaspedTime = iTime - iLastTime;
			iLastTime = iTime;
			float fElaspedTime = iElaspedTime * 0.001f;

			g_pInputManager->Manage();

			ImGui_ImplDX11_NewFrame();
			ImGui::Begin("Menu Debug");
			ImGui::Text("Hello World Imgui");
			ImGui::End();

			oFreeCamera.Update(g_pInputManager, fElaspedTime);
			const Matrix& oViewMatrix = oFreeCamera.GetViewMatrix();
			Matrix oProjMatrix = Matrix::CreatePerspectiveFieldOfView(M_PI / 4.0f, (float)WINDOW_WIDTH / (float)WINDOW_HEIGHT, 0.01f, 1000.0f);
			// Do a lot of thing like draw triangles with DirectX

			g_pImmediateContext->OMSetRenderTargets(1, &g_pRenderTargetView, g_pDepthStencilView);
			g_pImmediateContext->RSSetViewports(1, &vp);

			FLOAT rgba[] = { 0.2f, 0.2f, 0.2f, 0.0f };
			g_pImmediateContext->ClearRenderTargetView(g_pRenderTargetView, rgba);
			g_pImmediateContext->ClearDepthStencilView(g_pDepthStencilView, D3D11_CLEAR_DEPTH, 1.0, 0);
			// DRAW
			static float index = 0.0f; index += 0.001f;    // an ever-increasing float value
			Matrix worldViewProj;
			//worldViewProj = worldViewProj.CreateRotationX(index);	
			MonCB VsData;
			VsData.WorldViewProj = (worldViewProj * oViewMatrix * oProjMatrix).Transpose(); // Transpose car matrice gérée différemment entre shader et c++

			g_pImmediateContext->UpdateSubresource(g_pConstantBuffer, 0, NULL, &VsData.WorldViewProj, 0, 0);
			//g_pImmediateContext->VSSetConstantBuffers(0, 1, &g_pConstantBuffer);

			//g_pImmediateContext->Draw(3, 0);
			g_pImmediateContext->DrawIndexed((6 * (TailleX - 1) * (TailleY - 1)), 0, 0);
			//

			ImGui::Render();
			g_pSwapChain->Present(0, 0);
		}
	}
	//Release D3D objects
	ImGui_ImplDX11_Shutdown();
	g_pRenderTargetView->Release();
	g_pDepthStencilView->Release();
	g_pDepthStencil->Release();
	pRasterizerState->Release();
	g_pImmediateContext->Release();
	g_pSwapChain->Release();
	g_pDevice->Release();
	
	//My releases
	g_pConstantBuffer->Release();
	pVB->Release();
	pIndexBuffer->Release();
	pPixelShader->Release();
	pVertexShader->Release();
	pVertexLayout->Release();
	delete[] indices;
	delete[] vertexs;

	delete g_pInputManager;
	return (int) oMsg.wParam;
}

bool CreateDevice()
{
	UINT Flags = D3D11_CREATE_DEVICE_DEBUG;

	DXGI_SWAP_CHAIN_DESC sd;
	ZeroMemory(&sd, sizeof(sd));
	sd.BufferCount = 1;
	sd.BufferDesc.Width = WINDOW_WIDTH;
	sd.BufferDesc.Height = WINDOW_HEIGHT;
	sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	sd.BufferDesc.RefreshRate.Numerator = 0;// 60;
	sd.BufferDesc.RefreshRate.Denominator = 1;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sd.OutputWindow = hWnd;
	sd.SampleDesc.Count = 1;
	sd.SampleDesc.Quality = 0;
	sd.Windowed = true;
	sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
	sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

	HRESULT hr = D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, Flags, NULL, 0, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pDevice, NULL, &g_pImmediateContext);
	if (FAILED(hr))
		return false;
	ImGui_ImplDX11_Init(hWnd, g_pDevice, g_pImmediateContext);

	return true;
}

bool CreateDefaultRT()
{
	ID3D11Texture2D*	pBackBuffer;
	if (FAILED(g_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&pBackBuffer)))
		return false;

	HRESULT hr = g_pDevice->CreateRenderTargetView(pBackBuffer, NULL, &g_pRenderTargetView);
	pBackBuffer->Release();

	D3D11_TEXTURE2D_DESC descDepth;
	ZeroMemory(&descDepth, sizeof(descDepth));
	descDepth.Width = WINDOW_WIDTH;
	descDepth.Height = WINDOW_HEIGHT;
	descDepth.MipLevels = 1;
	descDepth.ArraySize = 1;
	descDepth.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	descDepth.SampleDesc.Count = 1;
	descDepth.SampleDesc.Quality = 0;
	descDepth.Usage = D3D11_USAGE_DEFAULT;
	descDepth.BindFlags = D3D11_BIND_DEPTH_STENCIL;
	descDepth.CPUAccessFlags = 0;
	descDepth.MiscFlags = 0;
	hr = g_pDevice->CreateTexture2D(&descDepth, NULL, &g_pDepthStencil);
	if (FAILED(hr))
		return false;

	// Create the depth stencil view
	D3D11_DEPTH_STENCIL_VIEW_DESC descDSV;
	ZeroMemory(&descDSV, sizeof(descDSV));
	descDSV.Format = descDepth.Format;
	descDSV.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	descDSV.Texture2D.MipSlice = 0;
	hr = g_pDevice->CreateDepthStencilView(g_pDepthStencil, &descDSV, &g_pDepthStencilView);
	if (FAILED(hr))
		return false;
	return true;
}

bool CreateCopyRT()
{
	D3D11_TEXTURE2D_DESC copyDepth;
	ZeroMemory(&copyDepth, sizeof(copyDepth));
	copyDepth.Width = WINDOW_WIDTH;
	copyDepth.Height = WINDOW_HEIGHT;
	copyDepth.MipLevels = 1;
	copyDepth.ArraySize = 1;
	copyDepth.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	copyDepth.SampleDesc.Count = 1;
	copyDepth.SampleDesc.Quality = 0;
	copyDepth.Usage = D3D11_USAGE_DEFAULT;
	copyDepth.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
	copyDepth.CPUAccessFlags = 0;
	copyDepth.MiscFlags = 0;
	HRESULT hr = g_pDevice->CreateTexture2D(&copyDepth, NULL, &g_pRender);
	if (FAILED(hr))
		return false;

	hr = g_pDevice->CreateRenderTargetView(g_pRender, NULL, &g_pRenderTargetViewCopy);
	g_pDevice->CreateShaderResourceView(g_pRender, NULL, &g_pShaderResourceViewCopy);

	if (FAILED(hr))
		return false;
	return true;
}

bool CompileShader(LPCWSTR pFileName, bool bPixel, LPCSTR pEntrypoint, ID3DBlob** ppCompiledShader)
{
	ID3DBlob* pErrorMsg = NULL;
	HRESULT hr = D3DCompileFromFile(pFileName, NULL, NULL, pEntrypoint, bPixel ? "ps_5_0" : "vs_5_0", 0, 0, ppCompiledShader, &pErrorMsg);

	if (FAILED(hr))
	{
		if (pErrorMsg != NULL)
		{
			OutputDebugStringA((char*)pErrorMsg->GetBufferPointer());
			pErrorMsg->Release();
		}
		return false;
	}
	if (pErrorMsg)
		pErrorMsg->Release();
	return true;
}

bool LoadRAW(const std::string& map)
{
	FILE *file;
	fopen_s(&file, map.c_str(), "rb");
	if (!file)
		return false;
	//lit la taille en X et en Y depuis le fichier
	fread(&m_sizeX, sizeof(unsigned short), 1, file);
	fread(&m_sizeY, sizeof(unsigned short), 1, file);
	unsigned int size = m_sizeX * m_sizeY;
	unsigned char *tmp = new unsigned char[size];
	m_height = new float[size];
	fread(tmp, sizeof(unsigned char), size, file);
	fclose(file);
	int i = 0;
	for (unsigned short y = 0; y < m_sizeY; ++y)
		for (unsigned short x = 0; x < m_sizeX; ++x, ++i)
			m_height[i] = float((m_maxZ * tmp[i]) / 255.0f);

	delete[] tmp;
	return true;
}