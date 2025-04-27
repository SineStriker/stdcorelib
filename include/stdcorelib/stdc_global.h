#ifndef STDCORELIB_STDC_GLOBAL_H
#define STDCORELIB_STDC_GLOBAL_H

#ifndef __has_attribute
# define __has_attribute(x) 0
#endif

#ifndef __has_builtin
# define __has_builtin(x) 0
#endif

#ifdef _WIN32
#  define STDCORELIB_DECL_EXPORT __declspec(dllexport)
#  define STDCORELIB_DECL_IMPORT __declspec(dllimport)
#  define STDCORELIB_DECL_HIDDEN
#else
#  define STDCORELIB_DECL_EXPORT __attribute__((visibility("default")))
#  define STDCORELIB_DECL_IMPORT __attribute__((visibility("default")))
#  define STDCORELIB_DECL_HIDDEN __attribute__((visibility("hidden")))
#endif

#ifndef STDCORELIB_EXPORT
#  ifdef STDCORELIB_STATIC
#    define STDCORELIB_EXPORT
#  else
#    ifdef STDCORELIB_LIBRARY
#      define STDCORELIB_EXPORT STDCORELIB_DECL_EXPORT
#    else
#      define STDCORELIB_EXPORT STDCORELIB_DECL_IMPORT
#    endif
#  endif
#endif

#define STDCORELIB_DISABLE_COPY(Class)                                                             \
    Class(const Class &) = delete;                                                                 \
    Class &operator=(const Class &) = delete;

#define STDCORELIB_DISABLE_MOVE(Class)                                                             \
    Class(Class &&) = delete;                                                                      \
    Class &operator=(Class &&) = delete;

#define STDCORELIB_DISABLE_COPY_MOVE(Class)                                                        \
    STDCORELIB_DISABLE_COPY(Class)                                                                 \
    STDCORELIB_DISABLE_MOVE(Class)

#if defined(__GNUC__) || defined(__clang__)
#  define STDCORELIB_PRINTF_FORMAT(fmtpos, attrpos)                                                \
      __attribute__((__format__(__printf__, fmtpos, attrpos)))
#else
#  define STDCORELIB_PRINTF_FORMAT(fmtpos, attrpos)
#endif

#if defined(_MSC_VER)
#  define STDCORELIB_NOINLINE __declspec(noinline)
#  define STDCORELIB_INLINE   __forceinline
#  define STDCORELIB_USED
#  define SYSCMDLINE_DEPRECATED __declspec(deprecated)
#else
#  define STDCORELIB_NOINLINE __attribute__((noinline))
#  define STDCORELIB_INLINE   __attribute__((always_inline))
#  define STDCORELIB_USED     __attribute__((used))
#  define SYSCMDLINE_DEPRECATED __attribute__((__deprecated__))
#endif

#if __has_builtin(__builtin_expect) || defined(__GNUC__)
#define STDCORELIB_LIKELY(EXPR) __builtin_expect((bool)(EXPR), true)
#define STDCORELIB_UNLIKELY(EXPR) __builtin_expect((bool)(EXPR), false)
#else
#define STDCORELIB_LIKELY(EXPR) (EXPR)
#define STDCORELIB_UNLIKELY(EXPR) (EXPR)
#endif

#ifndef _TSTR
#  ifdef _WIN32
#    define _TSTR(T) L##T
#  else
#    define _TSTR(T) T
#  endif
#endif

#endif // STDCORELIB_STDC_GLOBAL_H