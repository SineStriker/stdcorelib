#include "plugin.h"

#include "library.h"

namespace cpputils {

#ifdef _WIN32
#endif

    Plugin::~Plugin() = default;

    std::filesystem::path Plugin::path() const {
        return Library::locateLibraryPath(this);
    }

}