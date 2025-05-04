#ifndef STDCORELIB_SCOPE_GUARD_H
#define STDCORELIB_SCOPE_GUARD_H

#include <functional>
#include <type_traits>
#include <utility>

#include <stdcorelib/stdc_global.h>

namespace stdc {

    template <class F>
    class scope_guard {
    public:
        explicit scope_guard(F &&f) noexcept : m_func(std::move(f)) {
        }

        explicit scope_guard(const F &f) noexcept : m_func(f) {
        }

        scope_guard(scope_guard &&RHS) noexcept
            : m_func(std::move(RHS.m_func)), m_active(std::exchange(RHS.m_active, false)) {
        }

        ~scope_guard() noexcept {
            if (m_active)
                m_func();
        }

        void dismiss() noexcept {
            m_active = false;
        }

    protected:
        F m_func;
        bool m_active = true;

        STDCORELIB_DISABLE_COPY(scope_guard)
    };

    template <class F>
    scope_guard(F (&)()) -> scope_guard<F (*)()>;

    //! [make_scope_guard]
    template <typename F>
    [[nodiscard]] scope_guard<typename std::decay<F>::type> make_scope_guard(F &&f) {
        return scope_guard<typename std::decay<F>::type>(std::forward<F>(f));
    }

}

#endif // STDCORELIB_SCOPE_GUARD_H