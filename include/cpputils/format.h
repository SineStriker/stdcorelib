#ifndef CPPUTILS_FORMAT_H
#define CPPUTILS_FORMAT_H

#include <string>
#include <sstream>
#include <filesystem>

#include <cpputils/path.h>
#include <cpputils/codec.h>

namespace cpputils {

    template <class T>
    std::string anyToString(T &&t) {
        using T2 = std::decay_t<std::remove_cv_t<std::remove_reference_t<T>>>;
        if constexpr (std::is_same_v<T2, bool>) {
            return t ? "true" : "false";
        } else if constexpr (std::is_integral_v<T2>) {
            return std::to_string(t);
        } else if constexpr (std::is_floating_point_v<T2>) {
            std::ostringstream oss;
            oss << std::noshowpoint << t;
            return oss.str();
        } else if constexpr (std::is_same_v<T2, std::filesystem::path>) {
            return normalizePathSeparators(pathToString(t), true);
        } else if constexpr (std::is_same_v<T2, std::wstring>) {
            return wideToUtf8(t);
        } else if constexpr (std::is_same_v<T2, wchar_t *>) {
            return wideToUtf8(t);
        } else {
            return std::string(t);
        }
    }

    CPPUTILS_EXPORT std::string formatText(const std::string &format,
                                           const std::vector<std::string> &args);

    template <class... Args>
    auto formatTextN(const std::string &format, Args &&...args) {
        return formatText(format, {anyToString(std::forward<decltype(args)>(args))...});
    }

}

#endif // CPPUTILS_FORMAT_H
