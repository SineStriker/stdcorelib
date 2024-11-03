#ifndef STDCORELIB_WINDOWS_UTILS_H
#define STDCORELIB_WINDOWS_UTILS_H

#ifndef _WIN32
#  error "Unsupported system"
#endif

#include <Windows.h>

#include <string>

#include <stdcorelib/global.h>

namespace stdc {

    STDCORELIB_EXPORT std::wstring winErrorMessage(DWORD error, DWORD languageId);

    STDCORELIB_EXPORT std::wstring winGetFullDllDirectory();

    STDCORELIB_EXPORT std::wstring winGetFullModuleFileName(HMODULE hModule);

}

#endif // STDCORELIB_WINDOWS_UTILS_H