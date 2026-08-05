// SPDX-License-Identifier: MIT

#ifndef STDCORELIB_TEST_ANY_PLUGIN_H
#define STDCORELIB_TEST_ANY_PLUGIN_H

#include <stdcorelib/adt/any.h>

#ifdef _WIN32
#  define ANY_PLUGIN_API extern "C" __declspec(dllexport)
#else
#  define ANY_PLUGIN_API extern "C" __attribute__((visibility("default")))
#endif

/// Spelled out of this one header on both sides of the boundary, which is the case that decides
/// whether identity survives.
struct AnyPluginPayload {
    int value;
};

/// Puts a payload the plugin made into an any the caller owns.
ANY_PLUGIN_API void any_plugin_fill(stdc::any *out, int value);

/// Reads a payload the caller made, from inside the plugin.
ANY_PLUGIN_API bool any_plugin_read(const stdc::any *value, int *out);

/// The address of the plugin's own record for the payload type, so a test can tell whether the
/// two modules really do have separate ones.
ANY_PLUGIN_API const void *any_plugin_entry();

#endif // STDCORELIB_TEST_ANY_PLUGIN_H
