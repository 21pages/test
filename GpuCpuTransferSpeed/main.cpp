#include <d3d11.h>
#include <chrono>
#include <iostream>

#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3d11.lib")

using std::chrono::high_resolution_clock;

static void print_time(std::string tag, std::chrono::steady_clock::time_point start)
{
    auto end = high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    std::cout << tag << ": " << duration.count() * 1.0 / 1000 << "ms" << std::endl;

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
static bool all = true;

static HRESULT HOST_2_GPU(ID3D11DeviceContext* deviceContext, ID3D11Texture2D* defaultTexture)
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

    std::cout << "HOST -> GPU" << std::endl;
    auto start = high_resolution_clock::now();
    deviceContext->UpdateSubresource(defaultTexture, 0, &Box, data, width * 4, 4 * width * height);
    print_time("\tUpdateSubresource", start);

    delete[] data;

    return S_OK;
}

static HRESULT GPU_2_HOST(ID3D11DeviceContext* deviceContext, ID3D11Texture2D* defaultTexture)
{
    HRESULT hr;

    D3D11_TEXTURE2D_DESC desc;
    defaultTexture->GetDesc(&desc);
    ID3D11Device* device;
    deviceContext->GetDevice(&device);

    desc.Usage = D3D11_USAGE_STAGING;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    desc.BindFlags = 0;
    ID3D11Texture2D* stagingTexture;
    hr = device->CreateTexture2D(&desc, nullptr, &stagingTexture);
    CHECK_HR(hr);

    auto start = high_resolution_clock::now();
    auto begin = start;
    std::cout << "GPU -> HOST" << std::endl;
    deviceContext->CopyResource(stagingTexture, defaultTexture);
    print_time("\tCopyResource", start);

    D3D11_MAPPED_SUBRESOURCE ResourceDesc = {};
    start = high_resolution_clock::now();
    hr = deviceContext->Map(stagingTexture, 0, D3D11_MAP_READ, 0, &ResourceDesc);
    CHECK_HR(hr);
    print_time("\tMap", start);
    print_time("\tsum", begin);

    uint8_t* p = (uint8_t*)ResourceDesc.pData;
    for (int i = 0; i < height; i++) {
        for (int j = 0; j < width; j++) {
            if (p[i * width + j] != (uint8_t)(i + j)) {
                std::cerr << "read data mismatch !" << std::endl;
                return S_FALSE;
            }
        }
    }
    start = high_resolution_clock::now();
    deviceContext->Unmap(stagingTexture, 0);

    return S_OK;
}

static HRESULT GPU_2_GPU(ID3D11DeviceContext* deviceContext, ID3D11Texture2D* defaultTexture)
{
    HRESULT hr;

    D3D11_TEXTURE2D_DESC desc;
    defaultTexture->GetDesc(&desc);
    ID3D11Device* device;
    deviceContext->GetDevice(&device);

    ID3D11Texture2D* defaultTexture2;
    hr = device->CreateTexture2D(&desc, nullptr, &defaultTexture2);
    CHECK_HR(hr);

    auto start = high_resolution_clock::now();
    auto begin = start;
    std::cout << "GPU -> GPU" << std::endl;
    deviceContext->CopyResource(defaultTexture2, defaultTexture);
    print_time("\tCopyResource", start);

    return S_OK;
}

static HRESULT Adapter(IDXGIAdapter1* pAdapter)
{
    HRESULT hr;

    // Create a D3D11 device object
    ID3D11Device* device = NULL;
    ID3D11DeviceContext* deviceContext = NULL;
    const D3D_FEATURE_LEVEL lvl[] = { 
        D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0,
        D3D_FEATURE_LEVEL_9_3, D3D_FEATURE_LEVEL_9_2, D3D_FEATURE_LEVEL_9_1
    };
    uint32_t createFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
    // In Direct3D 11, if you are trying to create a hardware or a software device, set pAdapter != NULL which constrains the other inputs to be:
    // DriverType must be D3D_DRIVER_TYPE_UNKNOWN, Software must be NULL.
    hr = D3D11CreateDevice(pAdapter, 
        pAdapter ? D3D_DRIVER_TYPE_UNKNOWN : D3D_DRIVER_TYPE_HARDWARE,
        nullptr, createFlags, lvl, _countof(lvl),
        D3D11_SDK_VERSION, &device, nullptr, &deviceContext);
    CHECK_HR(hr);

    // Create a D3D11_TEXTURE2D_DESC structure for the texture
    D3D11_TEXTURE2D_DESC desc = {};
    ZeroMemory(&desc, sizeof(desc));
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = 0;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.MiscFlags = 0;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    desc.CPUAccessFlags = 0;

    // Create a default usage texture
    ID3D11Texture2D* defaultTexture;
    hr = device->CreateTexture2D(&desc, nullptr, &defaultTexture);
    CHECK_HR(hr);

    hr = HOST_2_GPU(deviceContext, defaultTexture);
    CHECK_HR(hr);

    hr = GPU_2_HOST(deviceContext, defaultTexture);
    CHECK_HR(hr);

    hr = GPU_2_GPU(deviceContext, defaultTexture);
    CHECK_HR(hr);

    return S_OK;
}

void usage()
{
    std::cout << "usage:" << "<exe> w h all" << std::endl;
    std::cout << "eg: <exe> 1920 1080 false" << std::endl;
    std::cout << "default: w=" << width << ", h=" << height << " all=" << all << std::endl;
}


int main(int argc, char ** argv)
{
    if (argc > 1 && argc != 4) {
        usage();
        return -1;
    }
    if (argc == 4) {
        width = atoi(argv[1]);
        height = atoi(argv[2]);
        all = strcmp(argv[3], "true") == 0;
    }

    HRESULT hr;

    std::cout << "width: " << width << " height: " << height << " all:" << (all ? "true" : "false") << std::endl;

    if (all) {
        IDXGIFactory1* pFactory = NULL;
        IDXGIAdapter1* pAdapter = NULL;

        hr = CreateDXGIFactory1(__uuidof(IDXGIFactory1), (void**)&pFactory);
        CHECK_HR(hr);
        for (UINT i = 0; pFactory->EnumAdapters1(i, &pAdapter) != DXGI_ERROR_NOT_FOUND; ++i)
        {
            DXGI_ADAPTER_DESC adesc;
            hr = pAdapter->GetDesc(&adesc);
            CHECK_HR(hr);
            std::wcout << "\n" << L"Adapter " << i + 1 << L": " << adesc.Description << std::endl;
            hr = Adapter(pAdapter);
            //CHECK_HR(hr);
        }
    }
    else {
        Adapter(NULL);
    }

    return 0;
}