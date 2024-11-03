#ifndef CPPUTILS_SYSTEM_H
#define CPPUTILS_SYSTEM_H

#include <string>
#include <vector>
#include <filesystem>

#include <cpputils/global.h>

namespace cpputils {

    class CPPUTILS_EXPORT System {
    public:
        static std::filesystem::path applicationPath();
        static std::filesystem::path applicationDirectory();
        static std::filesystem::path applicationFileName();
        static std::string applicationName();

        static std::vector<std::string> commandLineArguments();
    };

}

#endif // CPPUTILS_SYSTEM_H