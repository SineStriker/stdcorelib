#ifndef STDCORELIB_PIMPL_H
#define STDCORELIB_PIMPL_H

#include <memory>
#include <type_traits>

namespace stdc::pimpl {

    namespace _private_ {

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
        template <class _ThisType>
        struct get_decl_trait {
            using _ImplType = typename std::remove_pointer_t<_ThisType>;
            using _DeclType = typename _ImplType::Decl;
            using type = std::conditional_t<std::is_const_v<_ImplType>, const _DeclType, _DeclType>;
        };

    }

}

#define __stdc_impl_get(T)                                                                         \
    ::stdc::pimpl::_private_::get_impl_helper<typename T::Impl>(static_cast<T *>(this)->_impl)
#define __stdc_decl_get(T) static_cast<T *>(_decl)

#define __stdc_impl(T) auto &impl = *__stdc_impl_get(T)
#define __stdc_decl(T) auto &decl = *__stdc_decl_get(T)

#define __stdc_impl_t __stdc_impl(std::remove_pointer_t<decltype(this)>)
#define __stdc_decl_t __stdc_decl(::stdc::pimpl::_private_::get_decl_trait<decltype(this)>::type)

#endif // STDCORELIB_PIMPL_H