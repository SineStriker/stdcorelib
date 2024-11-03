#ifndef CPPUTILS_PLUGIN_H
#define CPPUTILS_PLUGIN_H

#include <filesystem>

#include <cpputils/global.h>

namespace cpputils {

    class CPPUTILS_EXPORT Plugin {
    public:
        virtual ~Plugin();

    public:
        virtual const char *iid() const = 0;
        virtual const char *key() const = 0;

    public:
        std::filesystem::path path() const;
    };

}

#define CPPUTILS_EXPORT_PLUGIN(PLUGIN_NAME)                                                        \
    extern "C" CPPUTILS_DECL_EXPORT cpputils::Plugin *cpputils_plugin_instance() {                 \
        static PLUGIN_NAME _instance;                                                              \
        return &_instance;                                                                         \
    }

#endif // PLUGIN_H
