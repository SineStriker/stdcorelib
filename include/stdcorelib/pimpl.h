#ifndef STDCORELIB_PIMPL_H
#define STDCORELIB_PIMPL_H

#include <memory>

namespace stdc_private::pimpl {

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

#define __stdc_impl_get(T) ::stdcorelib_private::pimpl::get<std::remove_const_t<T::Impl>>(_impl)
#define __stdc_decl_get(T) static_cast<T *>(_decl)

#define __stdc_impl(T) auto &impl = *__stdc_impl_get(T)
#define __stdc_decl(T) auto &decl = *__stdc_decl_get(T)

#define __stdc_impl_t __stdc_impl(std::remove_pointer_t<decltype(this)>)
#define __stdc_decl_t __stdc_decl(Decl)

#endif // STDCORELIB_PIMPL_H