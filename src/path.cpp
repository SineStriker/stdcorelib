#include "path.h"

#include <algorithm>

namespace fs = std::filesystem;

namespace stdc {

    namespace path {

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

    }

}