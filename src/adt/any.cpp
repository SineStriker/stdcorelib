// SPDX-License-Identifier: MIT

#include "any.h"

#include <mutex>
#include <set>
#include <string>

namespace stdc::detail {

    namespace {

        // The one table. Everything that makes two modules agree on a type goes through here, so
        // there has to be exactly one of these in a process.
        //
        // The names are owned copies rather than views into the callers' static strings, because
        // a module can be unloaded while the table outlives it. A set rather than a map, because
        // the address of the stored name is the answer and set nodes do not move.
        struct type_registry {
            std::mutex lock;
            std::set<std::string, std::less<>> names;
        };

        type_registry &registry() {
            static type_registry instance;
            return instance;
        }

    }

    const void *resolve_type_id(type_entry &entry) {
        // Written once and read forever after, so the lock is only touched on the first call per
        // type per module rather than on every comparison.
        if (const void *cached = entry.id.load(std::memory_order_acquire)) {
            return cached;
        }

        auto &table = registry();
        std::lock_guard<std::mutex> guard(table.lock);

        auto it = table.names.find(entry.name);
        if (it == table.names.end()) {
            it = table.names.emplace(entry.name).first;
        }

        const void *id = &*it;
        entry.id.store(id, std::memory_order_release);
        return id;
    }

}
