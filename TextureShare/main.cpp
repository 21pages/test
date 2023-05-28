#include <iostream>
#include <vector>
#include <cassert>
#include <d3d11.h>
#include <wrl/client.h>

#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3d11.lib")

using Microsoft::WRL::ComPtr;

#define HRE(hr) { if (FAILED((hr))) { std::cerr << "HR = " << (hr); exit(-1); } }

enum AdapterVendor {
	ADAPTER_VENDOR_AMD = 0x1002,
	ADAPTER_VENDOR_INTEL = 0x8086,
	ADAPTER_VENDOR_NVIDIA = 0x10DE,
};

static const int g_width = 1920;
static const int g_height = 1080;

static ComPtr<ID3D11Texture2D> CreateTexture(ID3D11Device * device)
{
	ComPtr<ID3D11Texture2D> texture = nullptr;
	D3D11_TEXTURE2D_DESC desc;

	ZeroMemory(&desc, sizeof(desc));
	desc.Width = g_width;
	desc.Height = g_height;
	desc.MipLevels = 0;
	desc.ArraySize = 1;
	desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	desc.SampleDesc.Count = 1;
	desc.SampleDesc.Quality = 0;
	desc.MiscFlags = D3D11_RESOURCE_MISC_SHARED;
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
	desc.CPUAccessFlags = 0;

	HRE(device->CreateTexture2D(&desc, nullptr, texture.ReleaseAndGetAddressOf()));

	return texture;
}

static ID3D11Device* CreateDevice(IDXGIAdapter *adapter) {
	if (!adapter) return nullptr;
	DXGI_ADAPTER_DESC desc;
	if (SUCCEEDED(adapter->GetDesc(&desc))) {
		//std::wcout << "Graphics adapter: " << desc.Description << std::endl;
	}
	ComPtr<ID3D11Device> dev = nullptr;
	UINT creationFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#ifdef _DEBUG
	creationFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
	auto hr = D3D11CreateDevice(
		adapter,
		(adapter ? D3D_DRIVER_TYPE_UNKNOWN : D3D_DRIVER_TYPE_HARDWARE),
		NULL, creationFlags, NULL, 0, D3D11_SDK_VERSION,
		dev.ReleaseAndGetAddressOf(), NULL,
		NULL);
	if (FAILED(hr)) {
		std::cerr << "D3D11CreateDevice failed, hr = " << std::hex << hr << std::dec << std::endl;
		return nullptr;
	}

	ComPtr<ID3D10Multithread> mt;
	hr = dev.As(&mt);
	if (FAILED(hr)) {
		std::cerr << "Get ID3D10Multithread failed, hr = " << std::hex << hr << std::dec << std::endl;
		return nullptr;
	}
	if (!mt->SetMultithreadProtected(TRUE) && !mt->GetMultithreadProtected()) {
		std::cerr << "SetMultithreadProtected failed" << std::endl;
		 return nullptr;
	}
	return dev.Detach();
}



static void SameAdapter(IDXGIAdapter *adapter) {
	ComPtr<ID3D11Device> dev1 = CreateDevice(adapter);
	ComPtr<ID3D11Device> dev2 = CreateDevice(adapter);
	ComPtr<ID3D11Texture2D> texture1 =  CreateTexture(dev1.Get());
	ComPtr<IDXGIResource> resource1 = nullptr;
	HRE(texture1.As(&resource1));
	HANDLE sharedHandle = nullptr;
	HRE(resource1->GetSharedHandle(&sharedHandle));

	ComPtr<IDXGIResource> resource2 = nullptr;
	HRE(dev2->OpenSharedResource(sharedHandle, __uuidof(ID3D10Texture2D), (void**)resource2.ReleaseAndGetAddressOf()));
	ComPtr<ID3D11Texture2D> texture2 = nullptr;
	HRE(resource2.As(&texture2));
	D3D11_TEXTURE2D_DESC desc;
	texture2->GetDesc(&desc);
	assert(desc.Width == g_width && desc.Height == g_height);
	//assert(texture1.Get() == texture2.Get());

	DXGI_ADAPTER_DESC aDesc;
	adapter->GetDesc(&aDesc);
	std::cout << "SameAdapter:OK, luid:" << aDesc.AdapterLuid.LowPart << std::endl;
}

static void LuidAdapter(IDXGIAdapter* adapter1, IDXGIAdapter* adapter2) {
	ComPtr<ID3D11Device> dev1 = CreateDevice(adapter1);
	ComPtr<ID3D11Device> dev2 = CreateDevice(adapter2);
	ComPtr<ID3D11Texture2D> texture1 = CreateTexture(dev1.Get());
	ComPtr<IDXGIResource> resource1 = nullptr;
	HRE(texture1.As(&resource1));
	HANDLE sharedHandle = nullptr;
	HRE(resource1->GetSharedHandle(&sharedHandle));

	ComPtr<IDXGIResource> resource2 = nullptr;
	HRE(dev2->OpenSharedResource(sharedHandle, __uuidof(ID3D10Texture2D), (void**)resource2.ReleaseAndGetAddressOf()));
	ComPtr<ID3D11Texture2D> texture2 = nullptr;
	HRE(resource2.As(&texture2));
	D3D11_TEXTURE2D_DESC desc;
	texture2->GetDesc(&desc);
	assert(desc.Width == g_width && desc.Height == g_height);

	DXGI_ADAPTER_DESC aDesc1;
	adapter1->GetDesc(&aDesc1);
	DXGI_ADAPTER_DESC aDesc2;
	adapter2->GetDesc(&aDesc2);
	std::cout << "LuidAdapter:OK, luid:" << aDesc1.AdapterLuid.LowPart << "," << aDesc2.AdapterLuid.LowPart << std::endl;
}

static std::vector<ComPtr<IDXGIAdapter>> GetAdapters(AdapterVendor vendor) {
	HRESULT hr = S_OK;

	ComPtr<IDXGIFactory1> factory1 = nullptr;
	std::vector<ComPtr<IDXGIAdapter>> adapters;

	HRE(CreateDXGIFactory1(__uuidof(IDXGIFactory1), (void**)factory1.ReleaseAndGetAddressOf()));

	ComPtr<IDXGIAdapter1> tmpAdapter = nullptr;
	for (int i = 0; !FAILED(factory1->EnumAdapters1(i, tmpAdapter.ReleaseAndGetAddressOf())); i++) {
		DXGI_ADAPTER_DESC1 desc = DXGI_ADAPTER_DESC1();
		tmpAdapter->GetDesc1(&desc);
		if (desc.VendorId == vendor) {
			ComPtr<IDXGIAdapter> tmp = tmpAdapter.Get();
			adapters.push_back(tmp);
		}
	}
	return adapters;
}



int main(int argv, char** argc) {
	
	std::vector<ComPtr<IDXGIAdapter>> adapters_1 = GetAdapters(ADAPTER_VENDOR_AMD);
	std::vector<ComPtr<IDXGIAdapter>> adapters_2 = GetAdapters(ADAPTER_VENDOR_AMD);

	for (auto& adapter : adapters_1) {
		SameAdapter(adapter.Get());
	}
	std::cout << "same luid" << std::endl;
	LuidAdapter(adapters_1[0].Get(), adapters_2[0].Get());

	// will fail
	if (adapters_1.size() > 1) {
		std::cout << "different luid" << std::endl;
		LuidAdapter(adapters_1[0].Get(), adapters_2[1].Get());
	}

	return 0;
}