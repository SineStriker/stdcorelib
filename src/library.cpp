#include "library.h"

#include <algorithm>
#include <cctype>

#ifdef _WIN32
#  include "windows_utils.h"
// 12345
#  include <Shlwapi.h>
#else
#  include <dlfcn.h>
#  include <limits.h>
#  include <string.h>
#endif

#include "codec.h"

#ifdef __APPLE__
#  define PRIOR_LIBRARY_PATH_KEY "DYLD_LIBRARY_PATH"
#else
#  define PRIOR_LIBRARY_PATH_KEY "LD_LIBRARY_PATH"
#endif

namespace fs = std::filesystem;

namespace cpputils {

    class Library::Impl {
    public:
        void *hDll = nullptr;
        fs::path path;

        virtual ~Impl();

        static inline int nativeLoadHints(int loadHints);
        static std::string sysErrorMessage(bool nativeLanguage);

        bool open(int hints = 0);
        bool close();
        void *resolve(const char *name) const;
    };

    Library::Impl::~Impl() {
        std::ignore = close();
    }

    inline int Library::Impl::nativeLoadHints(int loadHints) {
#ifdef _WIN32
        return 0;
#else
        int dlFlags = 0;
        if (loadHints & Library::ResolveAllSymbolsHint) {
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

    std::string Library::Impl::sysErrorMessage(bool nativeLanguage) {
#ifdef _WIN32
        return wideToUtf8(winErrorMessage(::GetLastError(), nativeLanguage));
#else
        auto err = dlerror();
        if (err) {
            return err;
        }
        return {};
#endif
    }

    bool Library::Impl::open(int hints) {
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

    bool Library::Impl::close() {
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

    void *Library::Impl::resolve(const char *name) const {
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
        Constructs a library.
     */
    Library::Library() : _impl(new Impl()) {
    }

    /*!
        Destroys the library object.
    */
    Library::~Library() = default;

    Library::Library(Library &&other) noexcept {
        std::swap(_impl, other._impl);
    }

    Library &Library::operator=(Library &&other) noexcept {
        if (this == &other)
            return *this;
        std::swap(_impl, other._impl);
        return *this;
    }

    /*!
        Loads the library and returns \c true if the library was loaded successfully.
    */
    bool Library::open(const fs::path &path, int hints) {
        _impl->path = path;
        if (_impl->open(hints)) {
            _impl->path = fs::canonical(fs::absolute(path));
            return true;
        }
        _impl->path.clear();
        return false;
    }

    /*!
        Unloads the library and returns \c true if the library could be unloaded.
    */
    bool Library::close() {
        if (_impl->close()) {
            _impl->path.clear();
            return true;
        }
        return false;
    }

    /*!
        Returns \c true if the library is open.
    */
    bool Library::isOpen() const {
        return _impl->hDll != nullptr;
    }

    /*!
        Returns the opened library path.
    */
    fs::path Library::path() const {
        return _impl->path;
    }

    /*!
        Returns the opened library handle.
    */
    void *Library::handle() const {
        return _impl->hDll;
    }

    /*!
        Returns the address of the exported symbol \a name, the library must be opened.
    */
    void *Library::resolve(const char *name) const {
        return _impl->resolve(name);
    }

    /*!
        Returns the error message of the last failed library operation.
    */
    std::string Library::lastError() const {
        return _impl->sysErrorMessage(false);
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
    bool Library::isLibrary(const fs::path &path) {
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
    fs::path Library::setLibraryPath(const fs::path &path) {
#ifdef _WIN32
        std::wstring org = winGetFullDllDirectory();
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
    fs::path Library::locateLibraryPath(const void *addr) {
#ifdef _WIN32
        HMODULE hModule = nullptr;
        if (!::GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                                      GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                  (LPCWSTR) addr, &hModule)) {
            return {};
        }
        return winGetFullModuleFileName(hModule);
#else
        Dl_info dl_info;
        dladdr(const_cast<void *>(addr), &dl_info);
        auto buf = dl_info.dli_fname;
        return buf;
#endif
    }

}