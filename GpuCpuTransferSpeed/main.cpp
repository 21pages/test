#include <d3d11.h>
#include <chrono>
#include <iostream>
#include <wrl/client.h>

#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3d11.lib")

using std::chrono::high_resolution_clock;
using Microsoft::WRL::ComPtr;

static void print_time(std::string tag, std::chrono::steady_clock::time_point start)
{
    auto end = high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    std::cout << tag << ": " << duration.count() * 1.0 / 1000 << "ms" << std::endl;
}

static long long us_since(std::chrono::steady_clock::time_point start)
{
    auto end = high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    return duration.count();
}



static std::string HrToString(HRESULT hr)
{
    char errorMsg[1024];
    DWORD flags = FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS;
    DWORD languageId = MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT);
    FormatMessageA(flags, nullptr, HRESULT_CODE(hr), languageId, errorMsg, sizeof(errorMsg), nullptr);
    return errorMsg;
}

#define CHECK_HR(hr) if(hr != S_OK) {std::cerr << "line: " << __LINE__ << "err hr: " << HrToString(hr) << std::endl; return S_FALSE;}

static int width = 1920;
static int height = 1080;
static int max_count = 100;
static bool all = true;
static long long host2gpu_us = 0;
static long long gpu2host_us = 0;
static long long gpu2host_us_copy = 0;
static long long gpu2host_us_map = 0;
static long long gpu2gpu_us = 0;

static HRESULT HOST_2_GPU(ComPtr<ID3D11DeviceContext> deviceContext, ComPtr<ID3D11Texture2D> defaultTexture)
{
    D3D11_BOX Box;
    Box.left = 0;
    Box.right = width;
    Box.top = 0;
    Box.bottom = height;
    Box.front = 0;
    Box.back = 1;

    uint8_t* data = new uint8_t[width * height * 4];
    for (int i = 0; i < height; i++) {
        for (int j = 0; j < width; j++) {
            data[i * width + j] = i + j;
        }
    }

    auto start = high_resolution_clock::now();
    deviceContext->UpdateSubresource(defaultTexture.Get(), 0, &Box, data, width * 4, 4 * width * height);
    host2gpu_us += us_since(start);

    delete[] data;

    return S_OK;
}

static HRESULT GPU_2_HOST(ComPtr<ID3D11DeviceContext> deviceContext, ComPtr<ID3D11Texture2D> defaultTexture)
{
    HRESULT hr;

    D3D11_TEXTURE2D_DESC desc;
    defaultTexture->GetDesc(&desc);
    ID3D11Device* device;
    deviceContext->GetDevice(&device);

    desc.Usage = D3D11_USAGE_STAGING;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    desc.BindFlags = 0;
    ComPtr<ID3D11Texture2D> stagingTexture;
    hr = device->CreateTexture2D(&desc, nullptr, &stagingTexture);
    CHECK_HR(hr);

    auto start = high_resolution_clock::now();
    auto begin = start;
    deviceContext->CopyResource(stagingTexture.Get(), defaultTexture.Get());
    gpu2host_us_copy += us_since(start);

    D3D11_MAPPED_SUBRESOURCE ResourceDesc = {};
    start = high_resolution_clock::now();
    hr = deviceContext->Map(stagingTexture.Get(), 0, D3D11_MAP_READ, 0, &ResourceDesc);
    CHECK_HR(hr);
    gpu2host_us_map += us_since(start);
    gpu2host_us += us_since(begin);

    uint8_t* p = (uint8_t*)ResourceDesc.pData;
    for (int i = 0; i < height; i++) {
        for (int j = 0; j < width; j++) {
            if (p[i * width + j] != (uint8_t)(i + j)) {
                std::cerr << "read data mismatch !" << std::endl;
                return S_FALSE;
            }
        }
    }
    deviceContext->Unmap(stagingTexture.Get(), 0);

    return S_OK;
}

static HRESULT GPU_2_GPU(ComPtr<ID3D11DeviceContext> deviceContext, ComPtr<ID3D11Texture2D> defaultTexture)
{
    HRESULT hr;

    D3D11_TEXTURE2D_DESC desc;
    defaultTexture->GetDesc(&desc);
    ComPtr<ID3D11Device> device;
    deviceContext->GetDevice(device.GetAddressOf());

    ComPtr<ID3D11Texture2D> defaultTexture2;
    hr = device->CreateTexture2D(&desc, nullptr, defaultTexture2.GetAddressOf());
    CHECK_HR(hr);

    auto start = high_resolution_clock::now();
    deviceContext->CopyResource(defaultTexture2.Get(), defaultTexture.Get());
    gpu2gpu_us += us_since(start);

    return S_OK;
}

static HRESULT Adapter(ComPtr<IDXGIAdapter1> pAdapter)
{
    HRESULT hr;

    // Create a D3D11 device object
    ComPtr<ID3D11Device> device = NULL;
    ComPtr<ID3D11DeviceContext> deviceContext = NULL;
    const D3D_FEATURE_LEVEL lvl[] = { 
        D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0,
        D3D_FEATURE_LEVEL_9_3, D3D_FEATURE_LEVEL_9_2, D3D_FEATURE_LEVEL_9_1
    };
    uint32_t createFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
    // In Direct3D 11, if you are trying to create a hardware or a software device, set pAdapter != NULL which constrains the other inputs to be:
    // DriverType must be D3D_DRIVER_TYPE_UNKNOWN, Software must be NULL.
    hr = D3D11CreateDevice(pAdapter.Get(),
        pAdapter ? D3D_DRIVER_TYPE_UNKNOWN : D3D_DRIVER_TYPE_HARDWARE,
        nullptr, createFlags, lvl, _countof(lvl),
        D3D11_SDK_VERSION, device.GetAddressOf(), nullptr, deviceContext.GetAddressOf());
    CHECK_HR(hr);

    // Create a D3D11_TEXTURE2D_DESC structure for the texture
    D3D11_TEXTURE2D_DESC desc = {};
    ZeroMemory(&desc, sizeof(desc));
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = 0;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.MiscFlags = 0;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    desc.CPUAccessFlags = 0;

    // Create a default usage texture
    ComPtr<ID3D11Texture2D> defaultTexture;
    hr = device->CreateTexture2D(&desc, nullptr, defaultTexture.GetAddressOf());
    CHECK_HR(hr);

    hr = HOST_2_GPU(deviceContext, defaultTexture);
    CHECK_HR(hr);

    hr = GPU_2_HOST(deviceContext, defaultTexture);
    CHECK_HR(hr);

    hr = GPU_2_GPU(deviceContext, defaultTexture);
    CHECK_HR(hr);

    return S_OK;
}

static HRESULT CallAdapter(ComPtr<IDXGIAdapter1> pAdapter)
{
    host2gpu_us = 0;
    gpu2host_us = 0;
    gpu2host_us_copy = 0;
    gpu2host_us_map = 0;
    gpu2gpu_us = 0;

    for (int i = 0; i < max_count; i++) {
        HRESULT hr = Adapter(pAdapter);
        CHECK_HR(hr);
    }
    std::cout << "HOST -> GPU: " << host2gpu_us * 1.0 / 1000 / max_count << "ms" << std::endl;
    std::cout << "GPU -> HOST: " << gpu2host_us * 1.0 / 1000 / max_count << "ms" << std::endl;
    std::cout << "\t\tCopyResource: " << gpu2host_us_copy * 1.0 / 1000 / max_count << "ms" << std::endl;
    std::cout << "\t\tMap: " << gpu2host_us_map * 1.0 / 1000 / max_count << "ms" << std::endl;
    std::cout << "GPU -> GPU: " << gpu2gpu_us * 1.0 / 1000 / max_count << "ms" << std::endl;

}

void usage()
{
    std::cout << "usage:" << "<exe> w h count all" << std::endl;
    std::cout << "eg: <exe> 1920 1080 100 false" << std::endl;
    std::cout << "default: w=" << width << ", h=" << height << " count=" << max_count << " all=" << (all ? "true" : "false") << std::endl;
}


int main(int argc, char ** argv)
{
    if (argc > 1 && argc != 5) {
        usage();
        return -1;
    }
    if (argc == 5) {
        width = atoi(argv[1]);
        height = atoi(argv[2]);
        max_count = atoi(argv[3]);
        all = strcmp(argv[4], "true") == 0;
    }

    HRESULT hr;

    std::cout << "width: " << width << " height: " << height << " count:" << max_count <<  " all:" << (all ? "true" : "false") << std::endl;



    if (all) {
        ComPtr<IDXGIFactory1> pFactory = NULL;
        ComPtr<IDXGIAdapter1> pAdapter = NULL;

        hr = CreateDXGIFactory1(__uuidof(IDXGIFactory1), (void**)pFactory.GetAddressOf());
        CHECK_HR(hr);
        for (UINT i = 0; pFactory->EnumAdapters1(i, pAdapter.GetAddressOf()) != DXGI_ERROR_NOT_FOUND; ++i)
        {
            DXGI_ADAPTER_DESC adesc;
            hr = pAdapter->GetDesc(&adesc);
            CHECK_HR(hr);
            std::wcout << "\n" << L"Adapter " << i + 1 << L": " << adesc.Description << std::endl;
            hr = CallAdapter(pAdapter);
            //CHECK_HR(hr);
        }
    }
    else {
        CallAdapter(NULL);
    }

    return 0;
}