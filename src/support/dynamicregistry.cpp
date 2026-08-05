// SPDX-License-Identifier: MIT

#include "dynamicregistry.h"

#include <map>
#include <mutex>
#include <string>

namespace stdc::detail {

    void *shared_instance(std::string_view name, void *(*create)()) {
        // As with the type table, there has to be exactly one of these in a process, and the
        // names are owned copies because the module that asked can be unloaded later.
        //
        // Nothing here is ever taken apart. What create() returns is deliberately left alone:
        // the destructor would belong to whichever module asked first, and calling it after that
        // module has been unloaded ends the process at exit, where there is nothing left on the
        // stack to say why.
        static std::mutex lock;
        static std::map<std::string, void *, std::less<>> instances;

        std::lock_guard<std::mutex> guard(lock);
        auto it = instances.find(name);
        if (it == instances.end()) {
            // Under the lock, so two threads racing to be first still get one object between
            // them. create() runs at most once per name.
            it = instances.emplace(std::string(name), create()).first;
        }
        return it->second;
    }

}
