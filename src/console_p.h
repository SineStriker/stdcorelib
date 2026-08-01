#ifndef STDCORELIB_CONSOLE_P_H
#define STDCORELIB_CONSOLE_P_H

#include <string>

#include <stdcorelib/console.h>

namespace stdc::console::detail {

    /// The attribute triple a terminal is in. A default-constructed one is both the state a
    /// terminal starts in and the state sgr_reset_sequence() returns it to.
    struct attributes {
        int style = nostyle;
        int fg = nocolor;
        int bg = nocolor;

        friend bool operator==(const attributes &lhs, const attributes &rhs) {
            return lhs.style == rhs.style && lhs.fg == rhs.fg && lhs.bg == rhs.bg;
        }
        friend bool operator!=(const attributes &lhs, const attributes &rhs) {
            return !(lhs == rhs);
        }
    };

    /// Returns the SGR escape sequence that takes a terminal from \a from to \a to, or an empty
    /// string when there is nothing to write. Pure: it touches no state and writes nowhere, which
    /// is the point -- it is the only part of the colour path a unit test can reach, since
    /// everything around it needs a real terminal.
    ///
    /// Two things to know about the output:
    ///   - Only attributes that actually differ are emitted.
    ///   - Only attributes being turned *on* produce a code. There is no code for clearing an
    ///     individual attribute, so a caller that needs to drop one pairs this with
    ///     sgr_reset_sequence() and then re-applies what should stay.
    ///
    /// \c black is not representable here and yields no code, matching what the markup has always
    /// done.
    STDCORELIB_EXPORT std::string sgr_sequence(const attributes &from, const attributes &to);

    /// Returns the sequence that puts a terminal back to its defaults, or an empty string when
    /// \a from already is the default.
    STDCORELIB_EXPORT std::string sgr_reset_sequence(const attributes &from);

}

#endif // STDCORELIB_CONSOLE_P_H
