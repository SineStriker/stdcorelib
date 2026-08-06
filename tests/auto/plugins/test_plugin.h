// SPDX-License-Identifier: MIT

#ifndef STDC_TEST_PLUGIN_H
#define STDC_TEST_PLUGIN_H

#include <stdcorelib/adt/any.h>
#include <stdcorelib/support/dynamicregistry.h>

#ifdef _WIN32
#  define TEST_PLUGIN_API extern "C" __declspec(dllexport)
#else
#  define TEST_PLUGIN_API extern "C" __attribute__((visibility("default")))
#endif

/// Spelled out of this one header on both sides of the boundary, which is the case that decides
/// whether identity survives.
struct AnyPluginPayload {
    int value;
};

/// The kind of thing a plugin registers. Declared here so that the host names the same type, and
/// therefore asks for the same registry.
struct PluginWidget {
    virtual ~PluginWidget() = default;
    virtual int tag() const = 0;
};

/// \name any
/// @{

/// Puts a payload the plugin made into an any the caller owns.
TEST_PLUGIN_API void any_plugin_fill(stdc::any *out, int value);

/// Reads a payload the caller made, from inside the plugin.
TEST_PLUGIN_API bool any_plugin_read(const stdc::any *value, int *out);

/// The address of the plugin's own record for the payload type, so a test can tell whether the
/// two modules really do have separate ones.
TEST_PLUGIN_API const void *any_plugin_entry();

/// @}

/// \name DynamicRegistry
/// @{

/// Registers an entry, from inside the plugin.
TEST_PLUGIN_API bool registry_plugin_add(const char *name, int tag);

/// How many entries the plugin can see, which is the question that matters.
TEST_PLUGIN_API size_t registry_plugin_size();

/// The address of the registry the plugin reached, to show whose it is.
TEST_PLUGIN_API const void *registry_plugin_address();

/// @}

#endif // STDC_TEST_PLUGIN_H
