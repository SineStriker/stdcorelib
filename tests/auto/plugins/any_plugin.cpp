// SPDX-License-Identifier: MIT

#include "any_plugin.h"

ANY_PLUGIN_API void any_plugin_fill(stdc::any *out, int value) {
    *out = AnyPluginPayload{value};
}

ANY_PLUGIN_API bool any_plugin_read(const stdc::any *value, int *out) {
    if (const auto *payload = stdc::any_cast<AnyPluginPayload>(value)) {
        *out = payload->value;
        return true;
    }
    return false;
}

ANY_PLUGIN_API const void *any_plugin_entry() {
    return &stdc::detail::entry_of<AnyPluginPayload>();
}
