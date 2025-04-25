#ifndef STDCORELIB_ALGORITHMS_H
#define STDCORELIB_ALGORITHMS_H

#include <cstddef>

#include <stdcorelib/stdc_global.h>

namespace stdc {

    template <typename ForwardIterator>
    void deleteAll(ForwardIterator begin, ForwardIterator end) {
        while (begin != end) {
            delete *begin;
            ++begin;
        }
    }

    template <typename Container>
    inline void deleteAll(const Container &c) {
        deleteAll(c.begin(), c.end());
    }

    inline constexpr size_t hash(size_t key, size_t seed = 0) noexcept {
        return size_t(key & (~0U)) ^ seed;
    }

    template <class Container, class T>
    inline bool contains(const Container &container, const T &key) {
#if __cplusplus >= 202002L
        return container.contains(key);
#else
        return container.count(key) != 0;
#endif
    }

}

#endif // STDCORELIB_ALGORITHMS_H
