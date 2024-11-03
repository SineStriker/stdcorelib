#include "path.h"

#include <algorithm>

#include "global.h"

namespace fs = std::filesystem;

namespace cpputils {

    /*!
        Returns path with directory separators normalized.
    */
    std::filesystem::path cleanPath(const std::filesystem::path &path) {
        fs::path result;
        for (const auto &part : path) {
            if (part == _TSTR("..")) {
                if (!result.empty() && result.filename() != _TSTR("..")) {
                    result = result.parent_path();
                } else {
                    result /= part;
                }
            } else if (part != _TSTR(".")) {
                result /= part;
            }
        }
        return result;
    }

    /*!
        Returns path with separators normalized as '/' or the one appropriate for the underlying
        operating system.
    */
    std::string normalizePathSeparators(const std::string &path, bool native) {
        std::string res = path;
#if _WIN32
        if (native) {
            std::replace(res.begin(), res.end(), '/', '\\');

        } else {
            std::replace(res.begin(), res.end(), '\\', '/');
        }
#else
        (void) native;
        std::replace(res.begin(), res.end(), '\\', '/');
#endif
        return res;
    }

    /*!
        \fn std::filesystem::path u8str2path(const std::string &s)

        Returns path converted from UTF-8 encoded string.
    */

    /*!
        \fn std::string path2u8str(const std::filesystem::path &path)

        Returns UTF-8 encoded string converted from path.
    */

}