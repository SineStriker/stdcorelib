// SPDX-License-Identifier: MIT

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

    SharedLibrary::SharedLibrary() : _impl(new Impl()) {
    }

    SharedLibrary::~SharedLibrary() = default;

    SharedLibrary::SharedLibrary(SharedLibrary &&other) noexcept = default;

    SharedLibrary &SharedLibrary::operator=(SharedLibrary &&other) noexcept = default;

    bool SharedLibrary::open(const fs::path &path, int hints) {
        stdc_impl_t;
        if (impl.hDll) {
            return true;
        }
        impl.path = path;
        if (impl.open(hints)) {
            impl.path = fs::canonical(fs::absolute(path));
            return true;
        }
        impl.path.clear();
        return false;
    }

    bool SharedLibrary::close() {
        stdc_impl_t;
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

    bool SharedLibrary::isOpen() const {
        stdc_impl_t;
        return impl.hDll != nullptr;
    }

    fs::path SharedLibrary::path() const {
        stdc_impl_t;
        return impl.path;
    }

    void *SharedLibrary::handle() const {
        stdc_impl_t;
        return impl.hDll;
    }

    void *SharedLibrary::resolve(const char *name) const {
        stdc_impl_t;
        return impl.resolve(name);
    }

    std::string SharedLibrary::lastError() const {
        return Impl::sysErrorMessage();
    }

    void SharedLibrary::release() {
        stdc_impl_t;
        impl.released = true;
    }

#if !defined(_WIN32) && !defined(__APPLE__)
    static bool checkVersionSuffix(const std::string_view &suffix) {
        if (suffix.empty() || suffix.front() == '.' || suffix.back() == '.') {
            return false;
        }
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
            if (part.empty() || !std::all_of(part.begin(), part.end(),
                                             [](unsigned char c) { return std::isdigit(c); })) {
                return false;
            }
        }
        return true;
    }
#endif

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
            return suffix.front() == '.' && suffix.size() > 1 &&
                   checkVersionSuffix(suffix.substr(1));
        }
        return false;
#endif
    }

    fs::path SharedLibrary::setLibraryPath(const fs::path &path) {
#ifdef _WIN32
        std::wstring org = winapi::kernel32::GetDllDirectoryW();
        ::SetDllDirectoryW(path.c_str());
#else
        std::string org;
        if (const char *env = std::getenv(PRIOR_LIBRARY_PATH_KEY); env) {
            org = env;
        }
        if (setenv(PRIOR_LIBRARY_PATH_KEY, path.string().c_str(), 1) != 0) {
            return {};
        }
#endif
        return org;
    }

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
        Dl_info dl_info{};
        if (!addr || dladdr(const_cast<void *>(addr), &dl_info) == 0 || !dl_info.dli_fname) {
            return {};
        }
        return dl_info.dli_fname;
#endif
    }

}
