#include "system.h"

#ifdef _WIN32
#  include "windows_utils.h"
#elif defined(__APPLE__)
#  include <crt_externs.h>
#else
#  include <fstream>
#endif

#include <algorithm>

#include "codec.h"

#ifdef _WIN32
using PathChar = wchar_t;
using PathString = std::wstring;
static constexpr const PathChar PathSeparator = L'\\';
#else
using PathChar = char;
using PathString = std::string;
static constexpr const PathChar PathSeparator = '/';
#endif

namespace cpputils {

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

    PathString __application_path() {
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

    static PathString __application_filename() {
        auto appName = __application_path();
        auto slashIdx = appName.find_last_of(PathSeparator);
        if (slashIdx != std::string::npos) {
            appName = appName.substr(slashIdx + 1);
        }
        return appName;
    }

    PathString __application_dir() {
        auto appDir = __application_path();
        auto slashIdx = appDir.find_last_of(PathSeparator);
        if (slashIdx != std::string::npos) {
            appDir = appDir.substr(0, slashIdx);
        }
        return appDir;
    }

    PathString __application_name() {
        auto appName = __application_filename();
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

    static inline std::string __path2str(const PathString &s) {
#ifdef _WIN32
        return wideToUtf8(s);
#else
        return s;
#endif
    }

    std::string System::applicationFileName() {
        static std::string result = __path2str(__application_filename());
        return result;
    }

    std::string System::applicationDirectory() {
        static std::string result = __path2str(__application_dir());
        return result;
    }

    std::string System::applicationPath() {
        static std::string result = __path2str(__application_path());
        return result;
    }

    std::string System::applicationName() {
        static std::string result = __path2str(__application_name());
        return result;
    }

    std::vector<std::string> System::commandLineArguments() {
        std::vector<std::string> res;
#ifdef _WIN32
        int argc;
        auto argvW = ::CommandLineToArgvW(::GetCommandLineW(), &argc);
        if (argvW == nullptr)
            return {};
        res.reserve(argc);
        for (int i = 0; i != argc; ++i) {
            res.push_back(wideToUtf8(argvW[i]));
        }
        ::LocalFree(argvW);
#elif defined(__APPLE__)
        auto argv = *(_NSGetArgv());
        for (int i = 0; argv[i] != nullptr; ++i) {
            res.push_back(argv[i]);
        }
#else
        std::ifstream file("/proc/self/cmdline", std::ios::in);
        if (file.fail())
            return {};
        std::string s;
        while (std::getline(file, s, '\0')) {
            res.push_back(s);
        }
        file.close();
#endif
        return res;
    }

}