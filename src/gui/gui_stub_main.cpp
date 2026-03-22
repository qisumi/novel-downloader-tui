#include <windows.h>

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
    MessageBoxW(
        nullptr,
        L"GUI frontend placeholder is active.\nReplace src/gui/gui_stub_main.cpp with the WinUI 3 app entry later.",
        L"novel-downloader-tui",
        MB_OK | MB_ICONINFORMATION);
    return 0;
}
