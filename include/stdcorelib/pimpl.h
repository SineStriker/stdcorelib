// SPDX-License-Identifier: MIT

#ifndef STDCORELIB_PIMPL_H
#define STDCORELIB_PIMPL_H

#include <memory>
#include <type_traits>

namespace stdc::pimpl::detail {

    // Unique Data
    template <class T, class T1 = T>
    inline const T *get_impl_helper(const std::unique_ptr<T1> &d) {
        return static_cast<const T *>(d.get());
    }

    template <class T, class T1 = T>
    inline T *get_impl_helper(std::unique_ptr<T1> &d) {
        return static_cast<T *>(d.get());
    }

    // Shared Data
    template <class T, class T1 = T>
    inline const T *get_impl_helper(const std::shared_ptr<T1> &d) {
        return static_cast<const T *>(d.get());
    }

    template <class T, class T1 = T>
    inline T *get_impl_helper(std::shared_ptr<T1> &d) {
        if (d.use_count() > 1) {
            d = std::make_shared<T>(*static_cast<T *>(d.get())); // detach
        }
        return static_cast<T *>(d.get());
    }

    // Raw pointer
    template <class T>
    inline T *get_impl_helper(T *ptr) {
        return ptr;
    }

    template <class T>
    inline const T *get_impl_helper(const T *ptr) {
        return ptr;
    }

    // Used by macros
    template <class ThisType>
    struct get_decl_trait {
        using ImplType = typename std::remove_pointer_t<ThisType>;
        using DeclType = typename ImplType::Decl;
        using type = std::conditional_t<std::is_const_v<ImplType>, const DeclType, DeclType>;
    };

}

#define stdc_impl_get(T)                                                                           \
    ::stdc::pimpl::detail::get_impl_helper<typename T::Impl>(static_cast<T *>(this)->_impl)
#define stdc_decl_get(T) static_cast<T *>(_decl)

#define stdc_impl(T) auto &impl = *stdc_impl_get(T)
#define stdc_decl(T) auto &decl = *stdc_decl_get(T)

#define stdc_impl_t stdc_impl(std::remove_pointer_t<decltype(this)>)
#define stdc_decl_t stdc_decl(::stdc::pimpl::detail::get_decl_trait<decltype(this)>::type)

#endif // STDCORELIB_PIMPL_H
