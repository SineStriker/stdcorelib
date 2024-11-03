#ifndef CPPUTILS_SYSTEM_H
#define CPPUTILS_SYSTEM_H

#include <string>
#include <vector>

#include <cpputils/global.h>

namespace cpputils {

    class CPPUTILS_EXPORT System {
    public:
        static std::string applicationFileName();
        static std::string applicationDirectory();
        static std::string applicationPath();
        static std::string applicationName();

        static std::vector<std::string> commandLineArguments();
    };

}

#endif // CPPUTILS_SYSTEM_H
