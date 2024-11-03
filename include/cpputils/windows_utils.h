#ifndef CPPUTILS_WINDOWS_UTILS_H
#define CPPUTILS_WINDOWS_UTILS_H

#ifndef _WIN32
#  error "Unsupported system"
#endif

#include <Windows.h>

#include <string>

#include <cpputils/global.h>

namespace cpputils {

    CPPUTILS_EXPORT std::wstring winErrorMessage(DWORD error, DWORD languageId);

    CPPUTILS_EXPORT std::wstring winGetFullDllDirectory();

    CPPUTILS_EXPORT std::wstring winGetFullModuleFileName(HMODULE hModule);

}

#endif // CPPUTILS_WINDOWS_UTILS_H