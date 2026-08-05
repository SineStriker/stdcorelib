// SPDX-License-Identifier: MIT

#include "test_plugin.h"

TEST_PLUGIN_API void any_plugin_fill(stdc::any *out, int value) {
    *out = AnyPluginPayload{value};
}

TEST_PLUGIN_API bool any_plugin_read(const stdc::any *value, int *out) {
    if (const auto *payload = stdc::any_cast<AnyPluginPayload>(value)) {
        *out = payload->value;
        return true;
    }
    return false;
}

TEST_PLUGIN_API const void *any_plugin_entry() {
    return &stdc::detail::entry_of<AnyPluginPayload>();
}

namespace {

    struct PluginWidgetImpl : PluginWidget {
        explicit PluginWidgetImpl(int t) : _tag(t) {
        }
        int tag() const override {
            return _tag;
        }
        int _tag;
    };

}

TEST_PLUGIN_API bool registry_plugin_add(const char *name, int tag) {
    return stdc::DynamicRegistry<PluginWidget>::instance().add(
        name, "registered by the plugin",
        [tag] { return std::unique_ptr<PluginWidget>(new PluginWidgetImpl(tag)); });
}

TEST_PLUGIN_API size_t registry_plugin_size() {
    return stdc::DynamicRegistry<PluginWidget>::instance().size();
}

TEST_PLUGIN_API const void *registry_plugin_address() {
    return &stdc::DynamicRegistry<PluginWidget>::instance();
}
