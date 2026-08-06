// SPDX-License-Identifier: MIT

#ifndef STDCORELIB_DYNAMICREGISTRY_H
#define STDCORELIB_DYNAMICREGISTRY_H

#include <algorithm>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <vector>

#include <stdcorelib/stdc_global.h>
#include <stdcorelib/adt/type_id.h>

namespace stdc {

    namespace detail {

        /// The one object this process keeps under \a name, built by \a create the first time
        /// anybody asks for it.
        ///
        /// A registry declared in a header cannot hold its own singleton: a function-local static
        /// in a template has one copy per module, so a plugin would register into a table the
        /// host never reads. What is exported here is a function, and a function belongs to the
        /// library, so every module that shares one copy of it resolves to the same object no
        /// matter what its own symbols are doing.
        ///
        /// \note What is built here is never destroyed, on purpose. Whichever module asks first
        ///       is the one that builds it, so a destructor would have to be called through a
        ///       pointer into that module, and a plugin that asked first and then unloaded would
        ///       take the process down at exit. Nothing is gained by running it either: the
        ///       memory goes back at exit regardless, and the entries a destructor would release
        ///       may belong to code that has already been unloaded.
        STDC_EXPORT void *shared_instance(std::string_view name, void *(*create)());

    }

    /// A registry that is filled at run time, looked up by name, and can be watched.
    ///
    /// The counterpart to StaticRegistry, for the entries a program only learns about once it is
    /// running: what a configuration file asked for, what a plugin directory turned out to hold,
    /// what a script defined. Registration is a call rather than a static object, so ordering is
    /// whatever the program decides.
    ///
    /// \code
    ///   auto &reg = stdc::DynamicRegistry<Codec>::instance();
    ///   reg.add("flac", "Free Lossless Audio Codec",
    ///           [] { return std::unique_ptr<Codec>(new FlacCodec()); });
    ///
    ///   if (auto entry = reg.find("flac")) {
    ///       auto codec = entry->instantiate();
    ///   }
    /// \endcode
    ///
    /// The registry is built on first use rather than during static initialization, so nothing
    /// depends on the order the translation units happen to be initialized in, and there is one
    /// of it per process rather than per module. See instance().
    ///
    /// \note Thread safe. add(), remove() and the lookups may be called from any thread.
    ///       Entries are handed out as shared_ptr, so one stays alive in the caller's hands even
    ///       if another thread removes it meanwhile.
    /// \warning The listener callbacks run while the registering thread is inside add() or
    ///          remove(). Calling back into the registry from one deadlocks.
    ///
    /// \sa StaticRegistry, for what is known at link time
    template <class T>
    class DynamicRegistry {
    public:
        using type = T;

        /// How to make one. A \c std::function rather than a plain pointer, since an entry
        /// discovered at run time usually has to carry something with it, such as the library it
        /// came out of.
        using Factory = std::function<std::unique_ptr<T>()>;

        /// One registered implementation. Owns its name, unlike the static registry's, because
        /// there is no literal to point at.
        class Entry {
        public:
            Entry(std::string name, std::string desc, Factory factory)
                : _name(std::move(name)), _desc(std::move(desc)), _factory(std::move(factory)) {
            }

            const std::string &name() const {
                return _name;
            }
            const std::string &desc() const {
                return _desc;
            }

            /// Makes one, or returns null if the factory declines.
            std::unique_ptr<T> instantiate() const {
                return _factory ? _factory() : nullptr;
            }

        private:
            std::string _name;
            std::string _desc;
            Factory _factory;
        };

        using EntryPointer = std::shared_ptr<const Entry>;

        /// Told when entries come and go. Install with add_listener().
        class Listener {
        public:
            virtual ~Listener() = default;

            virtual void entry_added(const EntryPointer &entry) {
                (void) entry;
            }
            virtual void entry_removed(const EntryPointer &entry) {
                (void) entry;
            }
        };

        /// The registry for \a T. Built on first use.
        ///
        /// One per process, not one per module: the static here caches a pointer, and the object
        /// it points at is owned by the table inside stdcorelib. A plugin that registers is
        /// therefore registering where the host will look.
        ///
        /// \note That holds as far as one copy of stdcorelib reaches. Two modules that each link
        ///       it statically have a table each, and then they have a registry each too.
        /// \note Never destroyed, which is what lets a plugin ask for it without becoming
        ///       responsible for taking it apart. See remove() for what a plugin does owe.
        static DynamicRegistry &instance() {
            static auto *self = static_cast<DynamicRegistry *>(
                detail::shared_instance(detail::type_name<DynamicRegistry>(),
                                        [] { return static_cast<void *>(new DynamicRegistry()); }));
            return *self;
        }

        /// Registers \a name.
        ///
        /// \retval false the name was taken, and nothing changed
        bool add(std::string name, std::string desc, Factory factory) {
            auto entry =
                std::make_shared<const Entry>(std::move(name), std::move(desc), std::move(factory));
            std::vector<Listener *> listeners;
            {
                std::unique_lock<std::shared_mutex> lock(_mutex);
                if (_entries.find(entry->name()) != _entries.end()) {
                    return false;
                }
                _entries.emplace(entry->name(), entry);
                listeners = _listeners;
            }
            for (Listener *listener : listeners) {
                listener->entry_added(entry);
            }
            return true;
        }

        /// Removes \a name.
        ///
        /// \retval false there was no such entry
        /// \note An instance already handed out by instantiate() is not affected. This only
        ///       stops new ones being made.
        ///
        /// \warning **A plugin has to do this before it is unloaded.** The factory it registered
        ///          is code inside the plugin, and the registry outlives the plugin, so an entry
        ///          left behind is a call into memory that is no longer mapped. Nothing detects
        ///          this: the entry looks exactly like any other until somebody instantiates it.
        ///
        /// \code
        ///   // in the plugin, on the way out
        ///   DynamicRegistry<Codec>::instance().remove("flac");
        /// \endcode
        ///
        /// The registry itself is not the plugin's to release. It is shared with the host and
        /// with every other plugin, and it is never destroyed, so the entries are the whole of
        /// what a plugin owns here.
        bool remove(std::string_view name) {
            EntryPointer entry;
            std::vector<Listener *> listeners;
            {
                std::unique_lock<std::shared_mutex> lock(_mutex);
                auto it = _entries.find(name);
                if (it == _entries.end()) {
                    return false;
                }
                entry = it->second;
                _entries.erase(it);
                listeners = _listeners;
            }
            for (Listener *listener : listeners) {
                listener->entry_removed(entry);
            }
            return true;
        }

        /// The entry registered under \a name, or null.
        EntryPointer find(std::string_view name) const {
            std::shared_lock<std::shared_mutex> lock(_mutex);
            auto it = _entries.find(name);
            return it == _entries.end() ? EntryPointer() : it->second;
        }

        /// Makes one directly, or returns null if \a name is not registered.
        std::unique_ptr<T> instantiate(std::string_view name) const {
            auto entry = find(name);
            return entry ? entry->instantiate() : nullptr;
        }

        /// Every entry, in name order. A snapshot, so iterating it is safe while another thread
        /// registers.
        std::vector<EntryPointer> entries() const {
            std::shared_lock<std::shared_mutex> lock(_mutex);
            std::vector<EntryPointer> result;
            result.reserve(_entries.size());
            for (const auto &pair : _entries) {
                result.push_back(pair.second);
            }
            return result;
        }

        size_t size() const {
            std::shared_lock<std::shared_mutex> lock(_mutex);
            return _entries.size();
        }

        /// Forgets everything. Mainly for a test that has to start from a known state.
        void clear() {
            std::unique_lock<std::shared_mutex> lock(_mutex);
            _entries.clear();
        }

        /// \name Watching
        /// @{

        /// Adds \a listener, which is not owned and has to outlive this registry or be removed
        /// first. Adding the same one twice does nothing.
        void add_listener(Listener *listener) {
            if (!listener) {
                return;
            }
            std::unique_lock<std::shared_mutex> lock(_mutex);
            if (std::find(_listeners.begin(), _listeners.end(), listener) == _listeners.end()) {
                _listeners.push_back(listener);
            }
        }

        void remove_listener(Listener *listener) {
            std::unique_lock<std::shared_mutex> lock(_mutex);
            _listeners.erase(std::remove(_listeners.begin(), _listeners.end(), listener),
                             _listeners.end());
        }

        /// @}

    private:
        DynamicRegistry() = default;

        mutable std::shared_mutex _mutex;

        // std::less<> so a string_view looks up without building a string first.
        std::map<std::string, EntryPointer, std::less<>> _entries;
        std::vector<Listener *> _listeners;

        STDC_DISABLE_COPY_MOVE(DynamicRegistry)
    };

}

#endif // STDCORELIB_DYNAMICREGISTRY_H
