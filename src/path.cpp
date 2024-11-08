#include "path.h"

#include <algorithm>

#include "global.h"

namespace fs = std::filesystem;

namespace stdc {

    /*！
        \namespace path
        \brief Namespace of path algorithms.
    */

    namespace path {

        /*!
            Returns path with directory separators normalized.
        */
        std::filesystem::path clean_path(const std::filesystem::path &path) {
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
        std::string normalize_separators(const std::string &path, bool native) {
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
            \fn std::filesystem::path from_utf8(const std::string &s)

            Returns path converted from UTF-8 encoded string.
        */

        /*!
            \fn std::string to_utf8(const std::filesystem::path &path)

            Returns UTF-8 encoded string converted from path.
        */

    }

}