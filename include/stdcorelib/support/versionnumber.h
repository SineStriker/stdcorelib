#ifndef VERSIONNUMBER_H
#define VERSIONNUMBER_H

#include <string>
#include <array>
#include <iostream>

#include <stdcorelib/stdc_global.h>

namespace stdc {

    class STDCORELIB_EXPORT VersionNumber {
    public:
        VersionNumber();
        explicit VersionNumber(int major, int minor = 0, int patch = 0, int tweak = 0);

        static VersionNumber fromString(const std::string_view &s);

    public:
        inline int major() const {
            return m_numbers[0];
        }

        inline int minor() const {
            return m_numbers[1];
        }

        inline int patch() const {
            return m_numbers[2];
        }

        inline int tweak() const {
            return m_numbers[3];
        }

        std::string toString() const;
        bool isEmpty() const;

        bool operator==(const VersionNumber &rhs) const;
        bool operator!=(const VersionNumber &rhs) const;
        bool operator<(const VersionNumber &rhs) const;
        bool operator>(const VersionNumber &rhs) const;
        bool operator<=(const VersionNumber &rhs) const;
        bool operator>=(const VersionNumber &rhs) const;

    private:
        std::array<int, 4> m_numbers;
    };

}

namespace std {

    template <>
    struct STDCORELIB_EXPORT hash<stdc::VersionNumber> {
        size_t operator()(const stdc::VersionNumber &key) const;
    };

    inline ostream &operator<<(std::ostream &out, const stdc::VersionNumber &c) {
        out << "VersionNumber(" << c.toString() << ")";
        return out;
    }

}

#endif // VERSIONNUMBER_H
