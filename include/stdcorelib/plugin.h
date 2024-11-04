#ifndef STDCORELIB_PLUGIN_H
#define STDCORELIB_PLUGIN_H

#include <filesystem>

#include <stdcorelib/global.h>

namespace stdc {

    class STDCORELIB_EXPORT Plugin {
    public:
        virtual ~Plugin();

    public:
        virtual const char *iid() const = 0;
        virtual const char *key() const = 0;

    public:
        std::filesystem::path path() const;
    };

}

#define STDCORELIB_EXPORT_PLUGIN(PLUGIN_NAME)                                                      \
    extern "C" STDCORELIB_DECL_EXPORT stdc::Plugin *stdcorelib_plugin_instance() {                 \
        static PLUGIN_NAME _instance;                                                              \
        return &_instance;                                                                         \
    }

#endif // PLUGIN_H