#include <Windows.h>
#include <stdio.h>
#include <iostream>
#include <Shobjidl.h>
#include <assert.h>

using namespace std;

#pragma comment(lib, "user32.lib")


// works when using color, greater win10 not supported but still works 
static void test_get_set_sys_color() {
    DWORD color = GetSysColor(COLOR_DESKTOP);
    printf("GetSysColor: {0x%x, 0x%x, 0x%x}\n",
        GetRValue(color),
        GetGValue(color),
        GetBValue(color));
    int elements[1] = { COLOR_BACKGROUND };
    color = RGB(GetRValue(color) + 0x20, GetGValue(color) + 0x40, GetBValue(color) + 0x60);
    DWORD colors[1] = { color };
    Sleep(10000);
    assert(SetSysColors(1, elements, colors));
}

// when old picture exists, it can recover
// when old picture doesn't exist, it can will back to the current background color
static void test_SystemParametersInfo() {

    wchar_t buf[MAX_PATH] = { 0 };
    assert(SystemParametersInfo(SPI_GETDESKWALLPAPER, MAX_PATH, buf, 0));
    std::wcout << buf << std::endl;
    assert(SystemParametersInfo(SPI_SETDESKWALLPAPER, 0, (PVOID)L"C:\\Users\\selfd\\Pictures\\wallpaper1.jpg", 0));
    Sleep(5000);
    std::cout << "set to NULL" << std::endl;
    assert(SystemParametersInfo(SPI_SETDESKWALLPAPER, 0, NULL, 0));
    Sleep(5000);
    std::cout << "set to old" << std::endl;
    assert(SystemParametersInfo(SPI_SETDESKWALLPAPER, 0, (PVOID)buf, 0));

}

int main(int argc, char** argv) {

    //test_get_set_sys_color();

    test_SystemParametersInfo();

	return 0;
}