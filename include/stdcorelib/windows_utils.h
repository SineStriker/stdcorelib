#ifndef STDCORELIB_WINDOWS_UTILS_H
#define STDCORELIB_WINDOWS_UTILS_H

#ifndef _WIN32
#  error "Unsupported system"
#endif

#include <Windows.h>

#include <string>
#include <string_view>
#include <cstdarg>

#include <stdcorelib/global.h>

namespace stdc {

    STDCORELIB_EXPORT std::wstring winFormatError(DWORD error, DWORD languageId,
                                                  va_list *args = nullptr);

    STDCORELIB_EXPORT std::wstring winGetFullDllDirectory();

    STDCORELIB_EXPORT std::wstring winGetFullModuleFileName(HMODULE hModule);

    STDCORELIB_EXPORT std::wstring win8bitToWide(const std::string_view &s, UINT cp, DWORD flags);

    STDCORELIB_EXPORT std::string winWideTo8bit(const std::wstring_view &s, UINT cp, DWORD flags);

}

#endif // STDCORELIB_WINDOWS_UTILS_H