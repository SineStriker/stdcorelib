#include "winapi.h"

#include "str.h"

namespace stdc::winapi {

    std::wstring kernel32::FormatMessageW(int from, LPCVOID source, DWORD message_id,
                                          DWORD language_id, bool ignore_inserts, void *arguments,
                                          bool is_array) {
        DWORD dwFlags = FORMAT_MESSAGE_ALLOCATE_BUFFER | from;
        if (ignore_inserts)
            dwFlags |= FORMAT_MESSAGE_IGNORE_INSERTS;
        if (is_array)
            dwFlags |= FORMAT_MESSAGE_ARGUMENT_ARRAY;

        std::wstring ret;
        wchar_t *string = nullptr;

        std::ignore = ::FormatMessageW(dwFlags, source, message_id, language_id, (LPWSTR) &string,
                                       0, (va_list *) arguments);
        ::LocalFree((HLOCAL) string);
        return ret;
    }

    std::wstring kernel32::MultiByteToWideChar(UINT cp, DWORD flags, const std::string_view &s) {
        auto size = ::MultiByteToWideChar(cp, flags, s.data(), int(s.size()), nullptr, 0);
        if (size <= 0) {
            return {};
        }
        std::wstring res;
        res.resize(size);
        std::ignore = ::MultiByteToWideChar(cp, flags, s.data(), int(s.size()), res.data(), size);
        return res;
    }

    std::string kernel32::WideCharToMultiByte(UINT cp, DWORD flags, const std::wstring_view &s) {
        auto size =
            ::WideCharToMultiByte(cp, flags, s.data(), int(s.size()), nullptr, 0, nullptr, nullptr);
        if (size <= 0) {
            return {};
        }
        std::string res;
        res.resize(size);
        std::ignore = ::WideCharToMultiByte(cp, flags, s.data(), int(s.size()), res.data(), size,
                                            nullptr, nullptr);
        return res;
    }

    std::wstring kernel32::GetDllDirectoryW() {
        auto size = ::GetDllDirectoryW(0, nullptr);
        if (size == 0) {
            return {};
        }

        std::wstring res;
        res.resize(size);
        if (!::GetDllDirectoryW(size, res.data())) {
            return {};
        }
        return res;
    }

    std::wstring kernel32::GetModuleFileNameW(HMODULE hModule) {
        // https://stackoverflow.com/a/57114164/17177007
        DWORD size = MAX_PATH;
        std::wstring buffer;
        buffer.resize(size);
        while (true) {
            DWORD result = ::GetModuleFileNameW(hModule, buffer.data(), size);
            if (result == 0) {
                break;
            }

            if (result < size) {
                buffer.resize(result);
                return buffer;
            }

            // Check if a larger buffer is needed
            if (::GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
                size *= 2;
                buffer.resize(size);
                continue;
            }

            // Exactly
            return buffer;
        }
        return {};
    }

    std::wstring kernel32::GetEnvironmentVariableW(LPCWSTR name, bool *ok) {
        DWORD size = ::GetEnvironmentVariableW(name, nullptr, 0);
        if (size == 0) {
            if (ok)
                *ok = GetLastError() == ERROR_ENVVAR_NOT_FOUND;
            return {};
        }
        std::wstring buffer;
        buffer.resize(size - 1);
        if (::GetEnvironmentVariableW(name, buffer.data(), size) == 0) {
            if (ok)
                *ok = false;
            return {};
        }
        if (ok)
            *ok = true;
        return buffer;
    }

    /*!
        Get formatted error message using \c FormatMessageW without ending line break.
    */
    std::wstring SystemError(DWORD error_code, DWORD language_id) {
        std::wstring ret =
            kernel32::FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM, nullptr, error_code, language_id);
        if (stdc::ends_with(ret, L"\r\n"))
            ret = ret.substr(0, ret.size() - 2);
        if (ret.empty()) {
            wchar_t buffer[50];
            std::ignore = wsprintfW(buffer, L"Unknown error %X.", unsigned(error_code));
            ret = buffer;
        }
        return ret;
    };

    static RTL_OSVERSIONINFOW static_get_real_os_version() {
        HMODULE hMod = ::GetModuleHandleW(L"ntdll.dll");
        using RtlGetVersionPtr = NTSTATUS(WINAPI *)(PRTL_OSVERSIONINFOW);
        auto pRtlGetVersion =
            reinterpret_cast<RtlGetVersionPtr>(::GetProcAddress(hMod, "RtlGetVersion"));
        RTL_OSVERSIONINFOW rovi{};
        rovi.dwOSVersionInfoSize = sizeof(rovi);
        pRtlGetVersion(&rovi);
        return rovi;
    }

    /*!
        Returns system version from \c ntdll.dll runtime library.
    */
    RTL_OSVERSIONINFOW SystemVersion() {
        static auto result = static_get_real_os_version();
        return result;
    }

}