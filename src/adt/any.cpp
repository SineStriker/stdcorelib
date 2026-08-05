// SPDX-License-Identifier: MIT

#include "any.h"

#include <map>
#include <mutex>
#include <string>

namespace stdc::detail {

    namespace {

        // The one table. Everything that makes two modules agree on a type goes through here, so
        // there has to be exactly one of these in a process.
        //
        // The keys are owned copies rather than views into the callers' static strings, because a
        // module can be unloaded while the table outlives it.
        struct type_registry {
            std::mutex lock;
            std::map<std::string, int, std::less<>> ids;
            int next = 1;
        };

        type_registry &registry() {
            static type_registry instance;
            return instance;
        }

    }

    int resolve_type_id(type_entry &entry) {
        // Written once and read forever after, so the lock is only on the first call per type per
        // module rather than on every comparison.
        if (int cached = entry.id.load(std::memory_order_acquire)) {
            return cached;
        }

        auto &table = registry();
        std::lock_guard<std::mutex> guard(table.lock);

        int id;
        auto it = table.ids.find(entry.name);
        if (it != table.ids.end()) {
            id = it->second;
        } else {
            id = table.next++;
            table.ids.emplace(entry.name, id);
        }

        entry.id.store(id, std::memory_order_release);
        return id;
    }

}
