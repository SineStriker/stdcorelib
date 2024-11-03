#include "windows_utils.h"

namespace cpputils {

    std::wstring winErrorMessage(DWORD error, DWORD languageId) {
        std::wstring rc;
        wchar_t *lpMsgBuf;

        DWORD len =
            ::FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                                 FORMAT_MESSAGE_IGNORE_INSERTS,
                             NULL, error, languageId, reinterpret_cast<LPWSTR>(&lpMsgBuf), 0, NULL);

        if (len) {
            // Remove tail line breaks
            if (lpMsgBuf[len - 1] == L'\n') {
                lpMsgBuf[len - 1] = L'\0';
                len--;
                if (len > 1 && lpMsgBuf[len - 1] == L'\r') {
                    lpMsgBuf[len - 1] = L'\0';
                    len--;
                }
            }
            rc = std::wstring(lpMsgBuf, int(len));
            ::LocalFree(lpMsgBuf);
        } else {
            rc += L"unknown error";
        }
        return rc;
    }

    std::wstring winGetFullDllDirectory() {
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

    std::wstring winGetFullModuleFileName(HMODULE hModule) {
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


}