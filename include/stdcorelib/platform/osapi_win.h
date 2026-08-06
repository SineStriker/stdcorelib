// SPDX-License-Identifier: MIT

#ifndef STDCORELIB_OSAPI_WIN_H
#define STDCORELIB_OSAPI_WIN_H

// WARNING: to be removed

// Disable min/max macros in windows headers
#define NOMINMAX

#include <windows.h>

#include <string>
#include <string_view>
#include <cstdarg>

#include <stdcorelib/stdc_global.h>

namespace stdc {

    STDC_EXPORT std::wstring winFormatError(DWORD error, DWORD languageId,
                                                  va_list *args = nullptr);

    STDC_EXPORT std::wstring winGetFullDllDirectory();

    STDC_EXPORT std::wstring winGetFullModuleFileName(HMODULE hModule);

    STDC_EXPORT std::wstring win8bitToWide(const std::string_view &s, UINT cp, DWORD flags);

    STDC_EXPORT std::string winWideTo8bit(const std::wstring_view &s, UINT cp, DWORD flags);

    STDC_EXPORT std::wstring winGetEnvironmentVariable(const wchar_t *name, bool *ok);

    STDC_EXPORT RTL_OSVERSIONINFOW winSystemVersion();

}

#endif // STDCORELIB_OSAPI_WIN_H