#ifndef STDCORELIB_VLA_H
#define STDCORELIB_VLA_H

#include <cstddef>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <cassert>
#include <iterator>
#include <type_traits>
#include <algorithm>

#if defined(_MSC_VER)
#  define _STDCORELIB_ALLOCA(size) _alloca(size)
#elif defined(__GNUC__) || defined(__clang__)
#  define _STDCORELIB_ALLOCA(size) alloca(size)
#endif

#ifdef _STDCORELIB_ALLOCA

/**
 * @brief Allocate a buffer on stack with the given type and size (Uninitialized).
 *
 */
#  define VLA_ALLOC(TYPE, NAME, SIZE)                                                              \
      TYPE *NAME = (TYPE *) _STDCORELIB_ALLOCA((SIZE) * sizeof(TYPE))

namespace stdc::vla::_private_ {

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
#  define VLA_NEW(TYPE, NAME, SIZE)                                                                \
      const size_t NAME##_VLA_SIZE__ = (SIZE);                                                     \
      VLA_ALLOC(TYPE, NAME, NAME##_VLA_SIZE__);                                                    \
      ::stdc::vla::_private_::ScopeGuard<TYPE> NAME##_VLA_GUARD__(NAME, NAME##_VLA_SIZE__);

#endif

namespace stdc {

    template <class T, int PreAlloc = 256>
    class vlarray {
    public:
        vlarray(int size = 0);
        ~vlarray();

        template <class InputIterator>
        vlarray(InputIterator first, InputIterator last);
        inline vlarray(std::initializer_list<T> args)
            : vlarray(args.begin(), args.end()) {
        }

        // copy constructor
        inline vlarray(const vlarray<T> &other) : s(other.s) {
            shared_copy_construct(other);
        }
        // move constructor
        inline vlarray(vlarray<T> &&other) noexcept : s(other.s) {
            shared_move_construct(std::forward<vlarray<T>>(other));
        }
        // template copy constructor
        template <int PreAlloc2>
        inline vlarray(const vlarray<T, PreAlloc2> &other) : s(other.s) {
            shared_copy_construct(other);
        }
        // template move constructor
        template <int Prealloc2>
        inline vlarray(vlarray<T, Prealloc2> &&other) noexcept : s(other.s) {
            shared_move_construct(other);
        }
        // copy assign
        vlarray &operator=(const vlarray<T> &other) {
            if (this == &other) {
                return *this;
            }
            shared_copy_assign(other);
            return *this;
        }
        // move assign
        vlarray &operator=(vlarray<T> &&other) noexcept {
            if (this == &other) {
                return *this;
            }
            shared_move_assign(std::forward<vlarray<T>>(other));
            return *this;
        }
        // template copy assign
        template <int Prealloc2>
        vlarray &operator=(const vlarray<T, Prealloc2> &other) {
            shared_copy_assign(other);
            return *this;
        }
        // template move assign
        template <int Prealloc2>
        vlarray &operator=(vlarray<T, Prealloc2> &&other) noexcept {
            shared_move_assign(std::forward<vlarray<T, Prealloc2>>(other));
            return *this;
        }

        template <int Prealloc2>
        inline bool operator==(const vlarray<T, Prealloc2> &other) const {
            if (s != other.s) {
                return false;
            }
            return std::equal(begin(), end(), other.begin(), other.end());
        }
        template <int Prealloc2>
        inline bool operator!=(const vlarray<T, Prealloc2> &other) const {
            return !operator==(other);
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

        inline T& first() { assert(s > 0); return *begin(); }
        inline const T& first() const { assert(s > 0); return *begin(); }
        inline T& last() { assert(s > 0); return *(end() - 1); }
        inline const T& last() const { assert(s > 0); return *(end() - 1); }
        inline bool isEmpty() const { return s == 0; }
        void resize(int size); // don't use this function

        inline T *data() { return ptr; }
        inline const T *data() const { return ptr; }
        inline const T * constData() const { return ptr; }

        using size_type = int;
        using value_type = T;
        using pointer = value_type *;
        using const_pointer = const value_type *;
        using reference = value_type &;
        using const_reference = const value_type &;
        using difference_type = std::ptrdiff_t;

        using iterator = T *;
        using const_iterator = const T *;
        using reverse_iterator = std::reverse_iterator<iterator>;
        using const_reverse_iterator = std::reverse_iterator<const_iterator>;

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
            char arr[PreAlloc];
            int64_t _align1;
            double _align2;
        };
        static constexpr const int StackLength = PreAlloc / sizeof(T);

        void alloc_impl(); // `s` must be set before calling, `ptr` will be set
        void free_impl();  // must reset `ptr` after calling

        inline bool is_stack_ptr() const {
            return ptr == reinterpret_cast<const T *>(arr);
        }
        inline void set_stack_ptr() {
            ptr = reinterpret_cast<T *>(arr);
        }

        template <int Prealloc2>
        void shared_copy_construct(const vlarray<T, Prealloc2> &other);
        template <int Prealloc2>
        void shared_move_construct(vlarray<T, Prealloc2> &&other);

        template <int Prealloc2>
        void shared_copy_assign(const vlarray<T, Prealloc2> &other);
        template <int Prealloc2>
        void shared_move_assign(vlarray<T, Prealloc2> &&other);

        void construct_new_impl();
        template <class InputIterator>
        void construct_copy_impl(InputIterator first, InputIterator last);
        template <class InputIterator>
        void construct_move_impl(InputIterator first, InputIterator last);
        void destruct_impl();

        template <class T1, int Prealloc2>
        friend class vlarray;
    };

    template <class T, int Prealloc>
    vlarray<T, Prealloc>::vlarray(int size) : s(size) {
        alloc_impl();
        construct_new_impl();
    }

    template <class T, int Prealloc>
    vlarray<T, Prealloc>::~vlarray() {
        destruct_impl();
        free_impl();
    }

    template <class T, int Prealloc>
    template <class InputIterator>
    vlarray<T, Prealloc>::vlarray(InputIterator first, InputIterator last)
        : s(std::distance(first, last)) {
        alloc_impl();
        construct_copy_impl(first, last);
    }

    template <class T, int Prealloc>
    void vlarray<T, Prealloc>::resize(int size) {
        if (size == s) {
            return;
        }

        // expand
        if (size > s) {
            if (size > StackLength) {
                auto new_ptr = reinterpret_cast<T *>(malloc(sizeof(T) * size));
                if constexpr (!std::is_trivial_v<T>) {
                    // move original
                    auto first = ptr;
                    auto last = ptr + s;
                    auto p = new_ptr;
                    while (first != last) {
                        new (p++) T(std::move(*(first++)));
                    }
                    auto q = new_ptr + size;
                    // create the rest
                    while (p != q) {
                        new (p++) T();
                    }
                    // destruct original
                    destruct_impl();
                } else {
                    memcpy(new_ptr, ptr, sizeof(T) * s);
                    memset(new_ptr + s, 0, sizeof(T) * (size - s));
                }
                free_impl();
                ptr = new_ptr;
                s = size;
            } else {
                if constexpr (!std::is_trivial_v<T>) {
                    auto p = ptr + s;
                    auto q = ptr + size;
                    while (p != q) {
                        new (p++) T();
                    }
                } else {
                    memset(ptr + s, 0, sizeof(T) * (size - s));
                }
                s = size;
            }
            return;
        }

        // shrink
        if (is_stack_ptr()) {
            auto p = ptr + s;
            auto q = ptr + size;
            // destruct redundant
            while (p-- != q) {
                p->~T();
            }
            s = size;
            return;
        }

        auto new_ptr = (size <= StackLength) ? reinterpret_cast<T *>(arr)
                                             : reinterpret_cast<T *>(malloc(sizeof(T) * size));
        if constexpr (!std::is_trivial_v<T>) {
            auto p = new_ptr;
            auto first = ptr;
            auto last = ptr + size;
            while (first != last) {
                new (p++) T(std::move(*(first++)));
            }
            // destruct original
            destruct_impl();
        } else {
            memcpy(new_ptr, ptr, sizeof(T) * size);
        }
        free(ptr);
        ptr = new_ptr;
        s = size;
    }

    template <class T, int Prealloc>
    void vlarray<T, Prealloc>::alloc_impl() {
        if (s <= StackLength) {
            ptr = reinterpret_cast<T *>(arr);
        } else {
            ptr = reinterpret_cast<T *>(malloc(sizeof(T) * s));
        }
    }

    template <class T, int Prealloc>
    void vlarray<T, Prealloc>::free_impl() {
        if (!is_stack_ptr()) {
            free(ptr);
        }
    }

    template <class T, int Prealloc>
    template <int Prealloc2>
    void vlarray<T, Prealloc>::shared_copy_construct(
        const vlarray<T, Prealloc2> &other) {
        alloc_impl();
        if constexpr (!std::is_trivial_v<T>) {
            construct_copy_impl(other.begin(), other.end());
        } else {
            memcpy(ptr, other.ptr, sizeof(T) * s);
        }
    }

    template <class T, int Prealloc>
    template <int Prealloc2>
    void vlarray<T, Prealloc>::shared_move_construct(vlarray<T, Prealloc2> &&other) {
        if (other.is_stack_ptr() || s < StackLength) {
            alloc_impl();
            if constexpr (!std::is_trivial_v<T>) {
                construct_move_impl(other.begin(), other.end());
            } else {
                memcpy(ptr, other.ptr, sizeof(T) * s);
            }
        } else {
            ptr = other.ptr;
            other.set_stack_ptr();
            other.s = 0;
        }
    }

    template <class T, int Prealloc>
    template <int Prealloc2>
    void
        vlarray<T, Prealloc>::shared_copy_assign(const vlarray<T, Prealloc2> &other) {
        destruct_impl();
        if (s != other.s) {
            free_impl();
            s = other.s;
            alloc_impl();
        }
        if constexpr (!std::is_trivial_v<T>) {
            construct_copy_impl(other.begin(), other.end());
        } else {
            memcpy(ptr, other.ptr, sizeof(T) * s);
        }
    }

    template <class T, int Prealloc>
    template <int Prealloc2>
    void vlarray<T, Prealloc>::shared_move_assign(vlarray<T, Prealloc2> &&other) {
        if (other.is_stack_ptr() || other.s < StackLength) {
            destruct_impl();
            if (s != other.s) {
                free_impl();
                s = other.s;
                alloc_impl();
            }
            if constexpr (!std::is_trivial_v<T>) {
                construct_move_impl(other.begin(), other.end());
            } else {
                memcpy(ptr, other.ptr, sizeof(T) * s);
            }
        } else {
            destruct_impl();
            free_impl();
            s = other.s;
            ptr = other.ptr;
            other.set_stack_ptr();
            other.s = 0;
        }
    }

    template <class T, int Prealloc>
    void vlarray<T, Prealloc>::construct_new_impl() {
        if constexpr (!std::is_trivial_v<T>) {
            auto p = ptr;
            auto q = p + s;
            while (p != q) {
                new (p++) T();
            }
        } else {
            memset(ptr, 0, s * sizeof(T));
        }
    }

    template <class T, int Prealloc>
    template <class InputIterator>
    void vlarray<T, Prealloc>::construct_copy_impl(InputIterator first, InputIterator last) {
        auto p = ptr;
        while (first != last) {
            new (p++) T(*(first++));
        }
    }

    template <class T, int Prealloc>
    template <class InputIterator>
    void vlarray<T, Prealloc>::construct_move_impl(InputIterator first, InputIterator last) {
        auto p = ptr;
        while (first != last) {
            new (p++) T(std::move(*(first++)));
        }
    }

    template <class T, int Prealloc>
    void vlarray<T, Prealloc>::destruct_impl() {
        if constexpr (!std::is_trivial_v<T>) {
            auto p = ptr + s;
            while (p-- != ptr) {
                p->~T();
            }
        }
    }

}

#endif // STDCORELIB_VLA_H