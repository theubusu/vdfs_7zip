#include "util.h"
#include <windows.h>
#include <string>
#include <cstdio>

FILE* g_logF = nullptr;

std::wstring getDLLlocation() {
    HMODULE hModule = NULL;

    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, (LPCWSTR)&getDLLlocation, &hModule);

    wchar_t path[MAX_PATH];
    GetModuleFileNameW(hModule, path, MAX_PATH);

    std::wstring fPath(path);
    size_t pos = fPath.find_last_of(L"\\/");

    return fPath.substr(0, pos);
};