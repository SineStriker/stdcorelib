#include "plugin.h"

#include "library.h"

namespace cpputils {

#ifdef _WIN32
#endif

    /*!
        Destroys a plugin instance.
    */
    Plugin::~Plugin() = default;

    /*!
        \fn const char *Plugin::iid() const;

        Returns the plugin interface id.
    */

    /*!
        \fn const char *Plugin::key() const;

        Returns the plugin key.
    */

    /*!
        Returns the plugin path.
    */
    std::filesystem::path Plugin::path() const {
        return Library::locateLibraryPath(this);
    }

}