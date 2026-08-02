#ifndef STDCORELIB_SHAREDLIBRARY_H
#define STDCORELIB_SHAREDLIBRARY_H

#include <memory>
#include <filesystem>

#include <stdcorelib/stdc_global.h>

namespace stdc {

    /// Loads a shared library at run time and resolves symbols from it. LoadLibraryEx on
    /// Windows, dlopen elsewhere. The library is unloaded when the object goes away, unless
    /// release() has given up ownership of the handle.
    ///
    /// \code
    ///   SharedLibrary lib;
    ///   if (!lib.open("/usr/lib/x86_64-linux-gnu/libc.so.6")) {
    ///       return lib.lastError();
    ///   }
    ///   auto fn = reinterpret_cast<void *(*) (size_t)>(lib.resolve("malloc"));
    /// \endcode
    class STDCORELIB_EXPORT SharedLibrary {
    public:
        SharedLibrary();
        ~SharedLibrary();

        SharedLibrary(SharedLibrary &&other) noexcept;
        SharedLibrary &operator=(SharedLibrary &&other) noexcept;

    public:
        /// Passed to open(). These map onto the dlopen flags and are ignored where the platform
        /// has no equivalent.
        enum LoadHint {
            ResolveAllSymbolsHint = 0x01,
            ExportExternalSymbolsHint = 0x02,
            LoadArchiveMemberHint = 0x04, // Unused
            PreventUnloadHint = 0x08,
            DeepBindHint = 0x10
        };

        /// Loads \a path, or returns false and leaves the reason in lastError(). \a hints is a
        /// bitwise or of LoadHint. An object that is already open is left alone.
        bool open(const std::filesystem::path &path, int hints = 0);

        /// Unloads the library. The OS only really unloads it once the last user has let go.
        bool close();

        bool isOpen() const;

        /// The path this was opened with, empty if it is not open.
        std::filesystem::path path() const;

        /// The underlying HMODULE or dlopen handle, for the platform calls this class does not
        /// wrap.
        void *handle() const;

        /// The address exported under \a name, or null with the reason in lastError(). The
        /// library must be open.
        void *resolve(const char *name) const;

        /// The reason the last operation on this object failed.
        std::string lastError() const;

        /// Gives up ownership, so the destructor will not unload. The handle stays valid and
        /// nobody will close it.
        void release();

        /// Whether \a path has a suffix this platform can load: .dll on Windows, .dylib on
        /// macOS, and .so with an optional numeric version behind it elsewhere.
        static bool isLibrary(const std::filesystem::path &path);

        /// Adds \a path to the search used for a library's own dependencies, and returns what
        /// was set before. This is what lets a plugin sitting outside the usual directories find
        /// the libraries next to it.
        static std::filesystem::path setLibraryPath(const std::filesystem::path &path);

        /// The path of the library that \a addr falls inside, which answers "where did this
        /// function come from" for an address handed back by resolve().
        static std::filesystem::path locateLibraryPath(const void *addr);

    protected:
        class Impl;
        std::unique_ptr<Impl> _impl;
    };

}

#endif // STDCORELIB_SHAREDLIBRARY_H