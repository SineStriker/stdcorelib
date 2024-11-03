#ifndef CPPUTILS_GLOBAL_H
#define CPPUTILS_GLOBAL_H

#ifdef _WIN32
#  define CPPUTILS_DECL_EXPORT __declspec(dllexport)
#  define CPPUTILS_DECL_IMPORT __declspec(dllimport)
#else
#  define CPPUTILS_DECL_EXPORT __attribute__((visibility("default")))
#  define CPPUTILS_DECL_IMPORT __attribute__((visibility("default")))
#endif

#ifndef CPPUTILS_EXPORT
#  ifdef CPPUTILS_STATIC
#    define CPPUTILS_EXPORT
#  else
#    ifdef CPPUTILS_LIBRARY
#      define CPPUTILS_EXPORT CPPUTILS_DECL_EXPORT
#    else
#      define CPPUTILS_EXPORT CPPUTILS_DECL_IMPORT
#    endif
#  endif
#endif

#define CPPUTILS_DISABLE_COPY(Class)                                                               \
    Class(const Class &) = delete;                                                                 \
    Class &operator=(const Class &) = delete;

#define CPPUTILS_DISABLE_MOVE(Class)                                                               \
    Class(Class &&) = delete;                                                                      \
    Class &operator=(Class &&) = delete;

#define CPPUTILS_DISABLE_COPY_MOVE(Class)                                                          \
    CPPUTILS_DISABLE_COPY(Class)                                                                   \
    CPPUTILS_DISABLE_MOVE(Class)

#if defined(__GNUC__) || defined(__clang__)
#  define CPPUTILS_PRINTF_FORMAT(fmtpos, attrpos)                                                  \
      __attribute__((__format__(__printf__, fmtpos, attrpos)))
#else
#  define CPPUTILS_PRINTF_FORMAT(fmtpos, attrpos)
#endif

#ifndef _TSTR
#  ifdef _WIN32
#    define _TSTR(T) L##T
#  else
#    define _TSTR(T) T
#  endif
#endif

#endif // CPPUTILS_GLOBAL_H