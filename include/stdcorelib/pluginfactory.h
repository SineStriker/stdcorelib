#ifndef STDCORELIB_PLUGINFACTORY_H
#define STDCORELIB_PLUGINFACTORY_H

#include <memory>
#include <filesystem>
#include <vector>

#include <stdcorelib/plugin.h>

namespace stdc {

    class STDCORELIB_EXPORT PluginFactory {
    public:
        PluginFactory();
        virtual ~PluginFactory();

    public:
        void addStaticPlugin(Plugin *plugin);
        std::vector<Plugin *> staticPlugins() const;

        void addPluginPath(const char *iid, const std::filesystem::path &path);
        void setPluginPaths(const char *iid, const std::vector<std::filesystem::path> &paths);
        const std::vector<std::filesystem::path> &pluginPaths(const char *iid) const;

    public:
        Plugin *plugin(const char *iid, const char *key) const;

        template <class T>
        inline T *plugin(const char *key) const;

    protected:
        class Impl;
        std::unique_ptr<Impl> _impl;

        explicit PluginFactory(Impl &impl);
    };

    template <class T>
    inline T *PluginFactory::plugin(const char *key) const {
        static_assert(std::is_base_of<Plugin, T>::value, "T should inherit from stdc::Plugin");
        return static_cast<T *>(plugin(reinterpret_cast<T *>(0)->T::iid(), key));
    }

}

#endif // STDCORELIB_PLUGINFACTORY_H
