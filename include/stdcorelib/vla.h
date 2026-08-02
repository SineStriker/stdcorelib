#ifndef STDCORELIB_VLA_H
#define STDCORELIB_VLA_H

#include <cstddef>
#include <cstdlib>
#include <new>

#if defined(_MSC_VER)
#  define _STDCORELIB_ALLOCA(size) _alloca(size)
#elif defined(__GNUC__) || defined(__clang__)
#  define _STDCORELIB_ALLOCA(size) alloca(size)
#endif

#ifdef _STDCORELIB_ALLOCA

/// Allocates an uninitialized buffer of \a SIZE elements of \a TYPE on the stack, and declares
/// \a NAME as a pointer to it.
///
/// \warning The storage dies with the enclosing function, not the enclosing scope, so a loop
///          that allocates on every pass keeps growing the frame. Nothing frees it either, which
///          is why the size has to be bounded by something the caller controls.
#  define VLA_ALLOC(TYPE, NAME, SIZE)                                                              \
      TYPE *NAME = (TYPE *) _STDCORELIB_ALLOCA((SIZE) * sizeof(TYPE))

namespace stdc::vla::detail {

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

/// Like VLA_ALLOC(), for a type that needs constructing. A guard in the same scope default
/// constructs the elements and destroys them on the way out.
///
/// \sa VLA_ALLOC()
#  define VLA_NEW(TYPE, NAME, SIZE)                                                                \
      const size_t NAME##_VLA_SIZE__ = (SIZE);                                                     \
      VLA_ALLOC(TYPE, NAME, NAME##_VLA_SIZE__);                                                    \
      ::stdc::vla::detail::ScopeGuard<TYPE> NAME##_VLA_GUARD__(NAME, NAME##_VLA_SIZE__);

#endif

#endif // STDCORELIB_VLA_H
