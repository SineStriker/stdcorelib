#ifndef STDCORELIB_VLA_H
#define STDCORELIB_VLA_H

#include <stddef.h>
#include <malloc.h>

#if defined(_MSC_VER)
#  define _STDCORELIB_ALLOCA(size) _alloca(size)
#elif defined(__GNUC__) || defined(__clang__)
#  define _STDCORELIB_ALLOCA(size) alloca(size)
#else
#  error "Unsupported compiler"
#endif

/**
 * @brief Allocate a buffer on stack with the given type and size (Uninitialized).
 *
 */
#define VLA_ALLOC(TYPE, NAME, SIZE) TYPE *NAME = (TYPE *) _STDCORELIB_ALLOCA((SIZE) * sizeof(TYPE))

#ifdef __cplusplus

namespace stdc_private::vla {

    template <class T>
    struct ScopeGuard {
        inline ScopeGuard(T *buf, size_t size) : buf_(buf), size_(size) {
            auto buf_end = buf + size;
            for (auto p = buf; p < buf_end; ++p) {
                new (p) T();
            }
        }
        inline ~ScopeGuard() {
            auto buf_end = buf_ + size_;
            for (auto p = buf_; p < buf_end; ++p) {
                p->~T();
            }
        }

    private:
        T *buf_;
        size_t size_;
    };

}

/**
 * @brief Allocate a C++ class array on stack with the given type and size (Initialized).
 *
 */
#  define VLA_NEW(TYPE, NAME, SIZE)                                                                \
      const size_t NAME##_VLA_SIZE__ = (SIZE);                                                     \
      VLA_ALLOC(TYPE, NAME, NAME##_VLA_SIZE__);                                                    \
      ::stdc_private::vla::ScopeGuard<TYPE> NAME##_VLA_GUARD__(NAME, NAME##_VLA_SIZE__);

#endif

#endif // STDCORELIB_VLA_H