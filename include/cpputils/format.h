#ifndef CPPUTILS_FORMAT_H
#define CPPUTILS_FORMAT_H

#include <string>
#include <vector>

#include <cpputils/stringutil.h>

namespace cpputils {

    CPPUTILS_EXPORT std::string formatText(const std::string &format,
                                           const std::vector<std::string> &args);

    template <class... Args>
    auto formatTextN(const std::string &format, Args &&...args) {
        return formatText(format, {any2str(std::forward<decltype(args)>(args))...});
    }

}

#endif // CPPUTILS_FORMAT_H
