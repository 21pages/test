#include <d3d11.h>
#include <chrono>
#include <iostream>

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

#define CHECK_HR(hr) if(hr != S_OK) {std::cout << "line: " << __LINE__ << "err hr: " << HrToString(hr) << std::endl; return -1;}

static int width = 1920;
static int height = 1080;


static HRESULT CPU_2_GPU(ID3D11DeviceContext* deviceContext, ID3D11Texture2D* defaultTexture)
{
    D3D11_BOX Box;
    Box.left = 0;
    Box.right = width;
    Box.top = 0;
    Box.bottom = height;
    Box.front = 0;
    Box.back = 1;

    uint8_t* data = new uint8_t[width * height * 4];

    std::cout << "CPU -> GPU" << std::endl;
    auto start = high_resolution_clock::now();
    deviceContext->UpdateSubresource(defaultTexture, 0, &Box, data, width * 4, 4 * width * height);
    print_time("\tUpdateSubresource", start);

    delete[] data;

    return S_OK;
}

static HRESULT GPU_2_CPU(ID3D11DeviceContext* deviceContext, ID3D11Texture2D* defaultTexture)
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
    std::cout << "\nGPU -> CPU" << std::endl;
    deviceContext->CopyResource(stagingTexture, defaultTexture);
    print_time("\tCopyResource", start);

    D3D11_MAPPED_SUBRESOURCE ResourceDesc = {};
    start = high_resolution_clock::now();
    hr = deviceContext->Map(stagingTexture, 0, D3D11_MAP_READ, 0, &ResourceDesc);
    CHECK_HR(hr);
    print_time("\tMap", start);
    start = high_resolution_clock::now();
    deviceContext->Unmap(stagingTexture, 0);
    print_time("\tUnmap", start);
    print_time("\tsum", begin);

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
    std::cout << "\nGPU -> GPU" << std::endl;
    deviceContext->CopyResource(defaultTexture2, defaultTexture);
    print_time("\tCopyResource", start);

    return S_OK;
}


int main()
{
    int width = 1920;
    int height = 1080;

    HRESULT hr;

    // Create a D3D11 device object
    ID3D11Device* device = NULL;
    ID3D11DeviceContext* deviceContext = NULL;
    hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0,
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

    std::cout << "width: " << width << " height: " << height << std::endl;

    hr = CPU_2_GPU(deviceContext, defaultTexture);
    CHECK_HR(hr);

    hr = GPU_2_CPU(deviceContext, defaultTexture);
    CHECK_HR(hr);

    hr = GPU_2_GPU(deviceContext, defaultTexture);
    CHECK_HR(hr);
}