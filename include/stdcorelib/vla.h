#ifndef STDCORELIB_VLA_H
#define STDCORELIB_VLA_H

#include <cstddef>
#include <cstdlib>
#include <cstdint>
#include <cassert>
#include <iterator>
#include <type_traits>

#if defined(_MSC_VER)
#  define _STDCORELIB_ALLOCA(size) _alloca(size)
#elif defined(__GNUC__) || defined(__clang__)
#  define _STDCORELIB_ALLOCA(size) alloca(size)
#else
#  error "Unsupported compiler"
#endif

/**
 * @brief Allocate a buffer on stack with the given type and size (Uninitialized).
 *
 */
#define VLA_ALLOC(TYPE, NAME, SIZE) TYPE *NAME = (TYPE *) _STDCORELIB_ALLOCA((SIZE) * sizeof(TYPE))

namespace stdc_private::vla {

    template <class T>
    struct ScopeGuard {
        inline ScopeGuard(T *buf, size_t size) : buf_(buf), size_(size) {
            auto buf_end = buf + size;
            for (auto p = buf; p < buf_end; ++p) {
                new (p) T();
            }
        }
        inline ~ScopeGuard() {
            auto buf_end = buf_ + size_;
            for (auto p = buf_; p < buf_end; ++p) {
                p->~T();
            }
        }

    private:
        T *buf_;
        size_t size_;
    };

}

/**
 * @brief Allocate a C++ class array on stack with the given type and size (Initialized).
 *
 */
#define VLA_NEW(TYPE, NAME, SIZE)                                                                  \
    const size_t NAME##_VLA_SIZE__ = (SIZE);                                                       \
    VLA_ALLOC(TYPE, NAME, NAME##_VLA_SIZE__);                                                      \
    ::stdc_private::vla::ScopeGuard<TYPE> NAME##_VLA_GUARD__(NAME, NAME##_VLA_SIZE__);

namespace stdc {

    template <class T, int Prealloc = 256>
    class VarLengthArray {
    public:
        static constexpr const int StackLength = Prealloc / sizeof(T);

        explicit VarLengthArray(int size) : s(size) {
            if (size <= StackLength) {
                ptr = reinterpret_cast<T *>(arr);
            } else {
                ptr = reinterpret_cast<T *>(malloc(sizeof(T) * size));
            }
            if constexpr (!std::is_trivial_v<T>) {
                auto p = ptr;
                auto q = p + size;
                while (p != q) {
                    new (p) T();
                    p++;
                }
            }
        }

        ~VarLengthArray() {
            if constexpr (!std::is_trivial_v<T>) {
                auto p = ptr + s;
                while (p-- != ptr) {
                    p->~T();
                }
            }
            if (ptr != reinterpret_cast<T *>(arr)) {
                free(ptr);
            }
        }

        // clang-format off
        inline T &operator[](int idx) {
            assert(idx >= 0 && idx < s);
            return ptr[idx];
        }
        inline const T &operator[](int idx) const {
            assert(idx >= 0 && idx < s);
            return ptr[idx];
        }
        inline const T &at(int idx) const { return operator[](idx); }

        inline int size() const { return s; }
        inline int count() const { return s; }
        inline int length() const { return s; }

        inline T *data() { return ptr; }
        inline const T *data() const { return ptr; }
        inline const T * constData() const { return ptr; }

        typedef int size_type;
        typedef T value_type;
        typedef value_type *pointer;
        typedef const value_type *const_pointer;
        typedef value_type &reference;
        typedef const value_type &const_reference;
        typedef std::ptrdiff_t difference_type;

        typedef T *iterator;
        typedef const T *const_iterator;
        typedef std::reverse_iterator<iterator> reverse_iterator;
        typedef std::reverse_iterator<const_iterator> const_reverse_iterator;

        inline iterator begin() { return ptr; };
        inline const_iterator begin() const { return ptr; }
        inline const_iterator cbegin() const { return ptr; }
        inline const_iterator constBegin() const { return ptr; }
        inline iterator end() { return ptr + s; }
        inline const_iterator end() const { return ptr + s; }
        inline const_iterator cend() const { return ptr + s; }
        inline const_iterator constEnd() const { return ptr + s; }
        reverse_iterator rbegin() { return reverse_iterator(end()); }
        reverse_iterator rend() { return reverse_iterator(begin()); }
        const_reverse_iterator rbegin() const { return const_reverse_iterator(end()); }
        const_reverse_iterator rend() const { return const_reverse_iterator(begin()); }
        const_reverse_iterator crbegin() const { return const_reverse_iterator(end()); }
        const_reverse_iterator crend() const { return const_reverse_iterator(begin()); }
        // clang-format on

    private:
        int s;  // size
        T *ptr; // data
        union {
            char arr[Prealloc];
            int64_t _align1;
            double _align2;
        };
    };

}

#endif // STDCORELIB_VLA_H