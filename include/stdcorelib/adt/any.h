// SPDX-License-Identifier: MIT

#ifndef STDCORELIB_ANY_H
#define STDCORELIB_ANY_H

#include <atomic>
#include <exception>
#include <memory>
#include <string_view>
#include <type_traits>
#include <utility>

#include <stdcorelib/stdc_global.h>

namespace stdc {

    namespace detail {

        /// The compiler's own spelling of \a T, cut out of the signature of this function.
        ///
        /// Identity rests on this text. Every module that mentions \a T gets its own copy of
        /// everything else here, and the spelling is the only part they are guaranteed to agree
        /// about.
        ///
        /// \note The spelling is whatever the compiler writes, not a normalized form: MSVC says
        ///       \c "struct Foo" where GCC says \c "Foo". That costs nothing inside a process,
        ///       which never holds two compilers at once, but it does mean the name is not a
        ///       portable key to write into a file.
        template <class T>
        constexpr std::string_view type_name() {
#if defined(_MSC_VER)
            constexpr std::string_view signature = __FUNCSIG__;
            constexpr std::string_view opening = "type_name<";
            constexpr auto first = signature.find(opening) + opening.size();
            constexpr auto last = signature.rfind(">(void)");
#else
            constexpr std::string_view signature = __PRETTY_FUNCTION__;
            constexpr std::string_view opening = "T = ";
            constexpr auto first = signature.find(opening) + opening.size();
            // GCC lists the other template parameters after a semicolon, clang does not.
            constexpr auto semicolon = signature.find(';', first);
            constexpr auto last =
                semicolon == std::string_view::npos ? signature.rfind(']') : semicolon;
#endif
            static_assert(first < last, "cannot read the type name out of this compiler");
            return signature.substr(first, last - first);
        }

        /// One of these exists per type per module.
        ///
        /// The address answers the question in the common case, the name answers it when two
        /// modules are involved, and \a id caches that second answer.
        struct type_entry {
            std::string_view name;
            std::atomic<int> id;
        };

        /// Gives \a entry the number every module in this process uses for that name.
        ///
        /// \note The table this reads lives in exactly one place, so it only unifies modules that
        ///       share one copy of the library. Statically linking stdcorelib into two of them
        ///       gives each its own table.
        STDCORELIB_EXPORT int resolve_type_id(type_entry &entry);

        template <class T>
        type_entry &entry_of() {
            static type_entry entry{type_name<T>(), 0};
            return entry;
        }

        inline bool same_type(type_entry &lhs, type_entry &rhs) {
            if (&lhs == &rhs) {
                return true; // one module, which is every comparison that never leaves home
            }
            return resolve_type_id(lhs) == resolve_type_id(rhs);
        }

        struct any_storage {
            virtual ~any_storage() = default;
            virtual std::unique_ptr<any_storage> clone() const = 0;
            virtual type_entry &type() const = 0;
        };

        template <class T>
        struct any_storage_impl : any_storage {
            explicit any_storage_impl(const T &v) : value(v) {
            }
            explicit any_storage_impl(T &&v) : value(std::move(v)) {
            }

            std::unique_ptr<any_storage> clone() const override {
                return std::unique_ptr<any_storage>(new any_storage_impl(value));
            }

            type_entry &type() const override {
                return entry_of<T>();
            }

            T value;
        };

    }

    class any;

    template <class T>
    const T *any_cast(const any *value) noexcept;

    template <class T>
    T *any_cast(any *value) noexcept;

    /// Holds a value of any copy constructible type, and remembers which type that was.
    ///
    /// The type is identified by the name the compiler gives it rather than by \c typeid, so this
    /// works with RTTI switched off, and a value keeps its identity across a shared library
    /// boundary where a scheme built on comparing addresses would not. \c std::any is the
    /// standard's answer to the same problem and is a fine choice when neither of those matters.
    ///
    /// \code
    ///   any value = std::string("text");
    ///   if (auto *s = any_cast<std::string>(&value)) {
    ///       use(*s);
    ///   }
    /// \endcode
    ///
    /// \warning Only the exact type comes back out. A value put in as \c Derived cannot be read
    ///          as \c Base, and const and reference qualifiers are stripped on the way in, so an
    ///          \c int and a \c const \c int& are the same type here.
    ///
    /// \note Every non-empty any owns a heap allocation. The object itself is one pointer, which
    ///       is what keeps a container of them small.
    ///
    /// \sa any_cast()
    class any {
    public:
        any() noexcept = default;
        ~any() = default;

        any(const any &other) : _storage(other._storage ? other._storage->clone() : nullptr) {
        }

        any(any &&other) noexcept = default;

        /// Takes a value of any type other than \c any itself.
        ///
        /// The parameter is disabled for \c any so that copying picks the copy constructor, and
        /// for anything an \c any converts to, which would otherwise recurse while the compiler
        /// works out whether that type is copy constructible.
        template <class T, std::enable_if_t<!std::is_same_v<std::decay_t<T>, any> &&
                                                !std::is_convertible_v<any, std::decay_t<T>> &&
                                                std::is_copy_constructible_v<std::decay_t<T>>,
                                            int> = 0>
        any(T &&value)
            : _storage(new detail::any_storage_impl<std::decay_t<T>>(std::forward<T>(value))) {
        }

        any &operator=(any other) noexcept {
            _storage = std::move(other._storage);
            return *this;
        }

        inline bool has_value() const noexcept {
            return _storage != nullptr;
        }

        inline void reset() noexcept {
            _storage.reset();
        }

        inline void swap(any &other) noexcept {
            _storage.swap(other._storage);
        }

        /// Whether the value in here is a \a T.
        template <class T>
        bool holds() const {
            return _storage &&
                   detail::same_type(_storage->type(), detail::entry_of<std::decay_t<T>>());
        }

        /// The compiler's name for the type held, or an empty view when there is no value.
        ///
        /// Meant for diagnostics. The spelling differs between compilers.
        inline std::string_view type_name() const {
            return _storage ? _storage->type().name : std::string_view();
        }

    private:
        std::unique_ptr<detail::any_storage> _storage;

        template <class T>
        friend const T *any_cast(const any *value) noexcept;
        template <class T>
        friend T *any_cast(any *value) noexcept;
    };

    /// \name Reading the value back
    ///
    /// The pointer forms answer with \c nullptr when the type does not match, and are the ones to
    /// reach for. The value form is a convenience for when the type is already known.
    /// @{

    template <class T>
    const T *any_cast(const any *value) noexcept {
        using U = std::decay_t<T>;
        if (!value || !value->holds<U>()) {
            return nullptr;
        }
        return &static_cast<detail::any_storage_impl<U> &>(*value->_storage).value;
    }

    template <class T>
    T *any_cast(any *value) noexcept {
        using U = std::decay_t<T>;
        if (!value || !value->holds<U>()) {
            return nullptr;
        }
        return &static_cast<detail::any_storage_impl<U> &>(*value->_storage).value;
    }

#ifdef STDCORELIB_EXCEPTIONS

    class bad_any_cast : public std::exception {
    public:
        inline const char *what() const noexcept override {
            return "stdc::bad_any_cast";
        }
    };

    /// \throws bad_any_cast when \a value does not hold a \a T
    template <class T>
    T any_cast(const any &value) {
        const auto *held = any_cast<std::remove_cv_t<std::remove_reference_t<T>>>(&value);
        if (!held) {
            throw bad_any_cast();
        }
        return static_cast<T>(*held);
    }

    /// \overload
    template <class T>
    T any_cast(any &value) {
        auto *held = any_cast<std::remove_cv_t<std::remove_reference_t<T>>>(&value);
        if (!held) {
            throw bad_any_cast();
        }
        return static_cast<T>(*held);
    }

#endif

    /// @}

    inline void swap(any &lhs, any &rhs) noexcept {
        lhs.swap(rhs);
    }

}

#endif // STDCORELIB_ANY_H
