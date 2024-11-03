#ifndef STDCORELIB_SYSTEM_H
#define STDCORELIB_SYSTEM_H

#include <string>
#include <vector>
#include <filesystem>

#include <stdcorelib/global.h>

namespace stdc {

    class STDCORELIB_EXPORT System {
    public:
        static std::filesystem::path applicationPath();
        static std::filesystem::path applicationDirectory();
        static std::filesystem::path applicationFileName();
        static std::string applicationName();

        static std::vector<std::string> commandLineArguments();
    };

}

#endif // STDCORELIB_SYSTEM_H