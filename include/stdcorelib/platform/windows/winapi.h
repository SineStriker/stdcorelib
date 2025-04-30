#ifndef STDCORELIB_WINAPI_H
#define STDCORELIB_WINAPI_H

// Disable min/max macros in windows headers
#ifndef NOMINMAX
#  define NOMINMAX
#endif

#include <windows.h>

#include <string>

#include <stdcorelib/stdc_global.h>

namespace stdc::winapi {

    struct STDCORELIB_EXPORT kernel32 {
        //
        // winbase
        //
        static std::wstring FormatMessageW(int from, LPCVOID source, DWORD message_id,
                                           DWORD language_id, bool ignore_inserts = true,
                                           void *arguments = nullptr, bool is_array = false);


        //
        // stringapiset
        //
        static std::wstring MultiByteToWideChar(UINT cp, DWORD flags, const std::string_view &s);
        static std::string WideCharToMultiByte(UINT cp, DWORD flags, const std::wstring_view &s);


        //
        // libloaderapi
        //
        static std::wstring GetDllDirectoryW();
        static std::wstring GetModuleFileNameW(HMODULE hModule);


        //
        // processenv
        //
        static std::wstring GetEnvironmentVariableW(LPCWSTR name, bool *exists);
    };

    struct STDCORELIB_EXPORT user32 {
        // To be added...
    };


    //
    // common
    //
    STDCORELIB_EXPORT std::wstring SystemError(DWORD error_code, DWORD language_id = 0);

    STDCORELIB_EXPORT RTL_OSVERSIONINFOW SystemVersion();

}

#endif // STDCORELIB_WINAPI_H