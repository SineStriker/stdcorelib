#include "sharedlibrary.h"

#include <algorithm>
#include <cctype>

#ifdef _WIN32
#  include "winapi.h"
#  include "winextra.h"
#else
#  include <dlfcn.h>
#  include <limits.h>
#  include <string.h>
#endif

#include "str.h"
#include "pimpl.h"

#ifdef __APPLE__
#  define PRIOR_LIBRARY_PATH_KEY "DYLD_LIBRARY_PATH"
#else
#  define PRIOR_LIBRARY_PATH_KEY "LD_LIBRARY_PATH"
#endif

namespace fs = std::filesystem;

namespace stdc {

    class SharedLibrary::Impl {
    public:
        void *hDll = nullptr;
        fs::path path;

        bool released = false;

        virtual ~Impl();

        static inline int nativeLoadHints(int loadHints);
        static std::string sysErrorMessage();

        bool open(int hints = 0);
        bool close();
        void *resolve(const char *name) const;
    };

    SharedLibrary::Impl::~Impl() {
        std::ignore = close();
    }

    inline int SharedLibrary::Impl::nativeLoadHints(int loadHints) {
#ifdef _WIN32
        return 0;
#else
        int dlFlags = 0;
        if (loadHints & ResolveAllSymbolsHint) {
            dlFlags |= RTLD_NOW;
        } else {
            dlFlags |= RTLD_LAZY;
        }
        if (loadHints & ExportExternalSymbolsHint) {
            dlFlags |= RTLD_GLOBAL;
        }
#  if !defined(Q_OS_CYGWIN)
        else {
            dlFlags |= RTLD_LOCAL;
        }
#  endif
#  if defined(RTLD_DEEPBIND)
        if (loadHints & DeepBindHint)
            dlFlags |= RTLD_DEEPBIND;
#  endif
        return dlFlags;
#endif
    }

    std::string SharedLibrary::Impl::sysErrorMessage() {
#ifdef _WIN32
        return wstring_conv::to_utf8(windows::SystemError(::GetLastError(), 0));
#else
        auto err = dlerror();
        if (err) {
            return err;
        }
        return {};
#endif
    }

    bool SharedLibrary::Impl::open(int hints) {
        auto absPath = fs::absolute(path);

        auto handle =
#ifdef _WIN32
            ::LoadLibraryW(absPath.c_str())
#else
            dlopen(absPath.c_str(), Impl::nativeLoadHints(hints))
#endif
            ;
        if (!handle) {
            return false;
        }

#ifdef _WIN32
        if (hints & PreventUnloadHint) {
            // prevent the unloading of this component
            HMODULE hmod;
            ::GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_PIN |
                                     GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
                                 reinterpret_cast<const wchar_t *>(handle), &hmod);
        }
#endif

        hDll = handle;
        return true;
    }

    bool SharedLibrary::Impl::close() {
        if (!hDll) {
            return true;
        }

        if (!
#ifdef _WIN32
            ::FreeLibrary(reinterpret_cast<HMODULE>(hDll))
#else
            (dlclose(hDll) == 0)
#endif
        ) {
            return false;
        }

        hDll = nullptr;
        return true;
    }

    void *SharedLibrary::Impl::resolve(const char *name) const {
        if (!hDll) {
            return nullptr;
        }

        auto addr =
#ifdef _WIN32
            ::GetProcAddress(reinterpret_cast<HMODULE>(hDll), name)
#else
            dlsym(hDll, name)
#endif
            ;
        return reinterpret_cast<void *>(addr);
    }

    /*!
        \class Library
        \brief Loads shared libraries at run time.

        This class provides a wrapper for dynamically loaded libraries. It provides a simple
        interface for opening, closing, and resolving symbols from the library.

        The interface is similar to the Qt QLibrary class.

        \sa https://doc.qt.io/qt-6/qlibrary.html
    */

    /*!
        Constructs a library.
     */
    SharedLibrary::SharedLibrary() : _impl(new Impl()) {
    }

    /*!
        Destroys the library object.
    */
    SharedLibrary::~SharedLibrary() = default;

    /*!
        Loads the library and returns \c true if the library was loaded successfully.
    */
    bool SharedLibrary::open(const fs::path &path, int hints) {
        __stdc_impl_t;
        if (impl.hDll) {
            std::ignore = close();
        }
        impl.path = path;
        if (impl.open(hints)) {
            impl.path = fs::canonical(fs::absolute(path));
            return true;
        }
        impl.path.clear();
        return false;
    }

    /*!
        Unloads the library and returns \c true if the library could be unloaded.
    */
    bool SharedLibrary::close() {
        __stdc_impl_t;
        if (impl.released) {
            impl.released = false;
            impl.hDll = nullptr;
            impl.path.clear();
            return true;
        }
        if (impl.close()) {
            impl.path.clear();
            return true;
        }
        return false;
    }

    /*!
        Returns \c true if the library is open.
    */
    bool SharedLibrary::isOpen() const {
        __stdc_impl_t;
        return impl.hDll != nullptr;
    }

    /*!
        Returns the opened library path.
    */
    fs::path SharedLibrary::path() const {
        __stdc_impl_t;
        return impl.path;
    }

    /*!
        Returns the opened library handle.
    */
    void *SharedLibrary::handle() const {
        __stdc_impl_t;
        return impl.hDll;
    }

    /*!
        Returns the address of the exported symbol \a name, the library must be opened.
    */
    void *SharedLibrary::resolve(const char *name) const {
        __stdc_impl_t;
        return impl.resolve(name);
    }

    /*!
        Returns the error message of the last failed library operation.
    */
    std::string SharedLibrary::lastError() const {
        return Impl::sysErrorMessage();
    }

    /*!
        Releases the library handle.
    */
    void SharedLibrary::release() {
        __stdc_impl_t;
        impl.released = true;
    }

#if !defined(_WIN32) && !defined(__APPLE__)
    static bool checkVersionSuffix(const std::string_view &suffix) {
        size_t start = 0;
        while (start < suffix.size()) {
            size_t dotPos = suffix.find('.', start);
            std::string_view part;
            if (dotPos == std::string::npos) {
                part = suffix.substr(start);
                start = suffix.size();
            } else {
                part = suffix.substr(start, dotPos - start);
                start = dotPos + 1;
            }
            if (!std::all_of(part.begin(), part.end(), ::isdigit)) {
                return false;
            }
        }
        return true;
    }
#endif

    /*!
        Returns \c true if \a path has a valid suffix for a loadable library.
    */
    bool SharedLibrary::isLibrary(const fs::path &path) {
#if defined(_WIN32)
        auto fileName = path.wstring();
        return fileName.size() >= 4 &&
               std::equal(fileName.end() - 4, fileName.end(), L".dll", [](wchar_t a, wchar_t b) {
                   return ::tolower(a) == ::tolower(b); //
               });
#elif defined(__APPLE__)
        auto fileName = path.string();
        return fileName.size() >= 6 &&
               std::equal(fileName.end() - 6, fileName.end(), L".dylib", [](char a, char b) {
                   return ::tolower(a) == ::tolower(b); //
               });
#else
        auto fileName = path.string();
        size_t soPos;
        if (fileName.size() >= 3 && (soPos = fileName.rfind(".so")) != std::string::npos) {
            // 检查 .so 后是否有版本号部分
            std::string_view suffix = std::string_view(fileName).substr(soPos + 3);
            if (suffix.empty()) {
                return true; // 仅有 .so，无版本号
            }
            return checkVersionSuffix(suffix); // 确保后缀全为数字
        }
        return false;
#endif
    }

    /*!
        Sets the library path hint as \a path, which is helpful when searching a loading library's
        dependencies.
    */
    fs::path SharedLibrary::setLibraryPath(const fs::path &path) {
#ifdef _WIN32
        std::wstring org = winapi::kernel32::GetDllDirectoryW();
        ::SetDllDirectoryW(path.c_str());
#else
        std::string org = getenv(PRIOR_LIBRARY_PATH_KEY);
        putenv((char *) (PRIOR_LIBRARY_PATH_KEY "=" + path.string()).c_str());
#endif
        return org;
    }

    /*!
        Returns the path of the library that the address \a addr locates in.
    */
    fs::path SharedLibrary::locateLibraryPath(const void *addr) {
#ifdef _WIN32
        HMODULE hModule = nullptr;
        if (!::GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                                      GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                  (LPCWSTR) addr, &hModule)) {
            return {};
        }
        return winapi::kernel32::GetModuleFileNameW(hModule);
#else
        Dl_info dl_info;
        dladdr(const_cast<void *>(addr), &dl_info);
        auto buf = dl_info.dli_fname;
        return buf;
#endif
    }

}