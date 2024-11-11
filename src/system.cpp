#include "system.h"

#ifdef _WIN32
#  include "windows_utils.h"
// ...
#  include <Psapi.h>
#elif defined(__APPLE__)
#  include <crt_externs.h>
#  include <mach/mach.h>
#  include <mach-o/dyld.h>
#else
#  include <limits.h>
#  include <unistd.h>
//
#  include <fstream>
#endif

#include <algorithm>

#include "path.h"

#ifdef _WIN32
using PathChar = wchar_t;
using PathString = std::wstring;
static constexpr const PathChar PathSeparator = L'\\';
#else
using PathChar = char;
using PathString = std::string;
static constexpr const PathChar PathSeparator = '/';
#endif

/*!
    \namespace stdc
    \brief Namespace of stdcorelib.
*/

namespace stdc {

#if defined(__APPLE__)

    static std::string macGetExecutablePath() {
        // Use stack buffer for the first try
        char stackBuf[PATH_MAX + 1];

        // "_NSGetExecutablePath" will return "-1" if the buffer is not large enough
        // and "*bufferSize" will be set to the size required.

        // Call
        unsigned int size = PATH_MAX + 1;
        char *buf = stackBuf;
        if (_NSGetExecutablePath(buf, &size) != 0) {
            // Re-alloc
            buf = new char[size]; // The return size contains the terminating 0

            // Call
            if (_NSGetExecutablePath(buf, &size) != 0) {
                delete[] buf;
                return {};
            }
        }

        // Return
        std::string res(buf);
        if (buf != stackBuf) {
            delete[] buf;
        }
        return res;
    }

#endif

    PathString sys_application_path() {
        static const auto res = []() -> PathString {
#ifdef _WIN32
            return winGetFullModuleFileName(nullptr);
#elif defined(__APPLE__)
            return macGetExecutablePath();
#else
            char buf[PATH_MAX];
            if (!realpath("/proc/self/exe", buf)) {
                return {};
            }
            return buf;
#endif
        }();
        return res;
    }

    static PathString sys_application_filename() {
        auto appName = sys_application_path();
        auto slashIdx = appName.find_last_of(PathSeparator);
        if (slashIdx != std::string::npos) {
            appName = appName.substr(slashIdx + 1);
        }
        return appName;
    }

    PathString sys_application_directory() {
        auto appDir = sys_application_path();
        auto slashIdx = appDir.find_last_of(PathSeparator);
        if (slashIdx != std::string::npos) {
            appDir = appDir.substr(0, slashIdx);
        }
        return appDir;
    }

    PathString sys_application_name() {
        auto appName = sys_application_filename();
#ifdef _WIN32
        auto dotIdx = appName.find_last_of(L'.');
        if (dotIdx != PathString::npos) {
            auto suffix = appName.substr(dotIdx + 1);
            std::transform(suffix.begin(), suffix.end(), suffix.begin(), ::tolower);
            if (suffix == L"exe") {
                appName = appName.substr(0, dotIdx);
            }
        }
#endif
        return appName;
    }

    static std::vector<std::string> sys_command_line_arguments() {
        std::vector<std::string> res;
#ifdef _WIN32
        int argc;
        auto argvW = ::CommandLineToArgvW(::GetCommandLineW(), &argc);
        if (argvW == nullptr)
            return {};
        res.reserve(argc);
        for (int i = 0; i != argc; ++i) {
            res.push_back(wstring_conv::to_utf8(argvW[i]));
        }
        ::LocalFree(argvW);
#elif defined(__APPLE__)
        auto argv = *(_NSGetArgv());
        for (int i = 0; argv[i] != nullptr; ++i) {
            res.push_back(argv[i]);
        }
#else
        std::ifstream file("/proc/self/cmdline", std::ios::in);
        if (!file.is_open())
            return {};
        std::string s;
        while (std::getline(file, s, '\0')) {
            res.push_back(s);
        }
        file.close();
#endif
        return res;
    }

    /*！
        \namespace system
        \brief Namespace of system related functions.
    */

    namespace system {

        /*!
            Returns the application file path.
        */
        std::filesystem::path application_path() {
            static std::filesystem::path result = sys_application_path();
            return result;
        }

        /*!
            Returns the application directory.
        */
        std::filesystem::path application_directory() {
            static std::filesystem::path result = sys_application_directory();
            return result;
        }

        /*!
            Returns the application file name.
        */
        std::filesystem::path application_filename() {
            static std::filesystem::path result = sys_application_filename();
            return result;
        }

        /*!
            Returns the application name in UTF-8 encoding.
        */
        std::string application_name() {
            static std::string result = path::to_utf8(sys_application_name());
            return result;
        }

        /*!
            Returns the command line arguments in UTF-8 encoding.
        */
        std::vector<std::string> command_line_arguments() {
            static auto result = sys_command_line_arguments();
            return result;
        }

    }

    /*
        Returns the memory usage of current process in bytes.
    */
    [[maybe_unused]] static size_t processMemoryUsage() {
#ifdef _WIN32
        PROCESS_MEMORY_COUNTERS pmc;
        if (::GetProcessMemoryInfo(::GetCurrentProcess(), &pmc, sizeof(pmc))) {
            return pmc.WorkingSetSize; // 以字节为单位
        }
        return 0;
#elif defined __APPLE__
        mach_task_basic_info info;
        mach_msg_type_number_t count = MACH_TASK_BASIC_INFO_COUNT;
        if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO, (task_info_t) &info, &count) ==
            KERN_SUCCESS) {
            return info.resident_size;
        }
        return 0;
#else
        std::ifstream statm("/proc/self/statm");
        if (!statm.is_open()) {
            return 0;
        }
        size_t size, resident, shared, text, lib, data, dt;
        statm >> size >> resident >> shared >> text >> lib >> data >> dt;
        return resident * sysconf(_SC_PAGESIZE);
#endif
    }

}