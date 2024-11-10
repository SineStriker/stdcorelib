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
            \fn std::string normalize_separators(const std::filesystem::path &path, bool native)

            Returns path with separators normalized as '/' or the one appropriate for the underlying
            operating system.
        */

        /*!
            \fn std::filesystem::path from_utf8(const std::string &s)

            Returns path converted from UTF-8 encoded string.
        */

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
            \fn std::string to_utf8(const std::filesystem::path &path)

            Returns UTF-8 encoded string converted from path.
        */

    }

}