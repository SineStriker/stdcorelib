#ifndef STDCORELIB_FORMAT_H
#define STDCORELIB_FORMAT_H

#include <string>
#include <vector>

#include <stdcorelib/stringutil.h>

namespace stdc {

    STDCORELIB_EXPORT std::string formatText(const std::string_view &format,
                                             const std::vector<std::string> &args);

    template <class... Args>
    auto formatTextN(const std::string_view &format, Args &&...args) {
        return formatText(format, {to_string(std::forward<decltype(args)>(args))...});
    }

}

#endif // STDCORELIB_FORMAT_H