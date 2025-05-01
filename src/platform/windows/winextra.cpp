#include "winextra.h"

#include "winapi.h"

#include "str.h"

namespace stdc {

    /*!
        \namespace windows
        \brief Windows API related functions and classes.
    */

}

namespace stdc::windows {

    using namespace winapi;

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
            std::ignore = wsprintfW(buffer, L"Unknown error 0x%X.", unsigned(error_code));
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

    // TODO: review
    std::chrono::system_clock::time_point FileTimeToTimePoint(const FILETIME &ft) {
        // 合并 64 位值
        const uint64_t filetime =
            (static_cast<uint64_t>(ft.dwHighDateTime) << 32) | ft.dwLowDateTime;

        // 定义时间差常量（1601→1970）
        constexpr uint64_t win_epoch_offset = 116444736000000000ULL; // 100ns 单位

        // 转换为 C++ chrono 的 duration 类型
        using filetime_duration =
            std::chrono::duration<int64_t, std::ratio<1, 10'000'000> // 精确表示 100ns
                                  >;

        // 计算从 1970 开始的 duration
        const auto since_epoch = filetime_duration(filetime - win_epoch_offset);

        // 转换为 system_clock 的时间点
        return std::chrono::system_clock::time_point(
            std::chrono::duration_cast<std::chrono::system_clock::duration>(since_epoch));
    }

    // TODO: review
    FILETIME TimePointToFileTime(const std::chrono::system_clock::time_point &tp) {
        // 计算与 FILETIME epoch 的时差
        constexpr auto epoch_diff = std::chrono::duration_cast<std::chrono::system_clock::duration>(
            std::chrono::duration<uint64_t, std::ratio<1, 10'000'000>>(11644473600ULL *
                                                                       10'000'000));

        const auto since_epoch = tp.time_since_epoch() + epoch_diff;
        const auto ft_duration =
            std::chrono::duration_cast<std::chrono::duration<uint64_t, std::ratio<1, 10'000'000>>>(
                since_epoch);

        // 转换为 FILETIME 结构
        ULARGE_INTEGER ull;
        ull.QuadPart = ft_duration.count();

        FILETIME ft;
        ft.dwLowDateTime = ull.LowPart;
        ft.dwHighDateTime = ull.HighPart;
        return ft;
    }

}