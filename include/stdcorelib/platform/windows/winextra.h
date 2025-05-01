#ifndef STDCORELIB_WINEXTRA_H
#define STDCORELIB_WINEXTRA_H

#include "stdc_windows.h"

#include <string>
#include <chrono>

#include <stdcorelib/stdc_global.h>

namespace stdc::windows {

    STDCORELIB_EXPORT std::wstring SystemError(DWORD error_code, DWORD language_id = 0);

    STDCORELIB_EXPORT RTL_OSVERSIONINFOW SystemVersion();

    STDCORELIB_EXPORT std::chrono::system_clock::time_point FileTimeToTimePoint(const FILETIME &ft);

    STDCORELIB_EXPORT FILETIME TimePointToFileTime(const std::chrono::system_clock::time_point &tp);

}

#endif // STDCORELIB_WINEXTRA_H