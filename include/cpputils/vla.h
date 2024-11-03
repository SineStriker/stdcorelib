#ifndef CPPUTILS_VLA_H
#define CPPUTILS_VLA_H

#include <stddef.h>
#include <malloc.h>

#if defined(_MSC_VER)
#  define _CPPUTILS_ALLOCA(size) _alloca(size)
#elif defined(__GNUC__) || defined(__clang__)
#  define _CPPUTILS_ALLOCA(size) alloca(size)
#else
#  error "Unsupported compiler"
#endif

/**
 * @brief Allocate a buffer on stack with the given type and size (Uninitialized).
 *
 */
#define VLA_ALLOC(TYPE, NAME, SIZE) TYPE *NAME = (TYPE *) _CPPUTILS_ALLOCA((SIZE) * sizeof(TYPE))

#ifdef __cplusplus

namespace cpputils_private::vla {

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
      VLA_ALLOC(TYPE, NAME, (SIZE));                                                               \
      ::cpputils_private::vla::ScopeGuard<TYPE> NAME##_VLA_GUARD__(NAME, (SIZE));

#endif

#endif // CPPUTILS_VLA_H
