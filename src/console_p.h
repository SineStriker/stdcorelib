// SPDX-License-Identifier: MIT

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
    /// string when there is nothing to write.
    ///
    /// Only attributes that differ are emitted, and only ones being turned *on*: there is no code
    /// for clearing an individual attribute, so a caller that needs to drop one pairs this with
    /// sgr_reset_sequence() and re-applies what should stay. \c black has no code at all.
    STDC_EXPORT std::string sgr_sequence(const attributes &from, const attributes &to);

    /// Returns the sequence that puts a terminal back to its defaults, or an empty string when
    /// \a from already is the default.
    STDC_EXPORT std::string sgr_reset_sequence(const attributes &from);

}

#endif // STDCORELIB_CONSOLE_P_H
