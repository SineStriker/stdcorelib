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

namespace stdc {

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
    /// The registry lives in a function-local static, so it is built on first use rather than
    /// during static initialization, and nothing depends on the order the translation units
    /// happen to be initialized in.
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
        static DynamicRegistry &instance() {
            static DynamicRegistry registry;
            return registry;
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

        STDCORELIB_DISABLE_COPY_MOVE(DynamicRegistry)
    };

}

#endif // STDCORELIB_DYNAMICREGISTRY_H
