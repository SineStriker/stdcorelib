// SPDX-License-Identifier: MIT

#include "path.h"

#include <algorithm>

namespace fs = std::filesystem;

namespace stdc {

    namespace path {

        std::filesystem::path clean_path(const std::filesystem::path &path) {
            fs::path result;
            for (const auto &part : path) {
                if (part == STDCORELIB_TSTR("..")) {
                    if (!result.empty() && result.filename() != STDCORELIB_TSTR("..")) {
                        result = result.parent_path();
                    } else {
                        result /= part;
                    }
                } else if (part != STDCORELIB_TSTR(".")) {
                    result /= part;
                }
            }
            return result;
        }

    }

}