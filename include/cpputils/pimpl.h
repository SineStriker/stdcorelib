#ifndef CPPUTILS_PIMPL_H
#define CPPUTILS_PIMPL_H

#include <memory>

namespace cpputils_private::pimpl {

    // Unique Data
    template <class T, class T1 = T>
    inline const T *get(const std::unique_ptr<T1> &d) {
        return static_cast<const T *>(d.get());
    }

    template <class T, class T1 = T>
    inline T *get(std::unique_ptr<T1> &d) {
        return static_cast<T *>(d.get());
    }

    // Shared Data
    template <class T, class T1 = T>
    inline const T *get(const std::shared_ptr<T1> &d) {
        return static_cast<const T *>(d.get());
    }

    template <class T, class T1 = T>
    inline T *get(std::shared_ptr<T1> &d) {
        if (d.use_count() > 1) {
            d = std::make_shared<T>(*static_cast<T *>(d.get())); // detach
        }
        return static_cast<T *>(d.get());
    }

}

#define __cpputils_impl_get(T) ::cpputils_private::pimpl::get<std::remove_const_t<T::Impl>>(_impl)
#define __cpputils_decl_get(T) static_cast<T *>(_decl)

#define __cpputils_impl(T) auto &impl = *__cpputils_impl_get(T)
#define __cpputils_decl(T) auto &decl = *__cpputils_decl_get(T)

#define __cpputils_impl_t __cpputils_impl(std::remove_pointer_t<decltype(this)>)
#define __cpputils_decl_t __cpputils_decl(Decl)

#endif // CPPUTILS_PIMPL_H
