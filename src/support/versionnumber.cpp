#include "versionnumber.h"

#include <cstdlib>
#include <charconv>
#include <typeinfo>

#include "algorithms.h"

namespace stdc {

    VersionNumber::VersionNumber() {
        m_numbers[0] = 0;
        m_numbers[1] = 0;
        m_numbers[2] = 0;
        m_numbers[3] = 0;
    }

    VersionNumber::VersionNumber(int major, int minor, int patch, int tweak) {
        m_numbers[0] = major;
        m_numbers[1] = minor;
        m_numbers[2] = patch;
        m_numbers[3] = tweak;
    }

    VersionNumber VersionNumber::fromString(const std::string_view &s) {
        VersionNumber version;

        // Split the string by '.' and convert each segment to int
        int i = 0;
        std::string::size_type start = 0;
        std::string::size_type end = s.find('.');
        while (i < version.m_numbers.size() && end != std::string::npos) {
            std::string_view segment = s.substr(start, end - start);
            std::ignore = std::from_chars(segment.data(), segment.data() + segment.size(),
                                          version.m_numbers[i++]);
            start = end + 1;
            end = s.find('.', start);
        }
        if (i < version.m_numbers.size() && start < s.size()) {
            std::string_view segment = s.substr(start);
            std::ignore = std::from_chars(segment.data(), segment.data() + segment.size(),
                                          version.m_numbers[i++]);
        }
        return version;
    }

    std::string VersionNumber::toString() const {
        if (tweak() != 0) {
            return std::to_string(major()) + "." + std::to_string(minor()) + "." +
                   std::to_string(patch()) + "." + std::to_string(tweak());
        }
        if (patch() != 0) {
            return std::to_string(major()) + "." + std::to_string(minor()) + "." +
                   std::to_string(patch());
        }
        return std::to_string(major()) + "." + std::to_string(minor());
    }

    bool VersionNumber::isEmpty() const {
        return major() == 0 && minor() == 0 && patch() == 0 && tweak() == 0;
    }

    bool VersionNumber::operator==(const VersionNumber &rhs) const {
        return major() == rhs.major() && minor() == rhs.minor() && patch() == rhs.patch() &&
               tweak() == rhs.tweak();
    }

    bool VersionNumber::operator!=(const VersionNumber &rhs) const {
        return !(*this == rhs);
    }

    bool VersionNumber::operator<(const VersionNumber &rhs) const {
        if (major() < rhs.major())
            return true;
        if (major() > rhs.major())
            return false;
        if (minor() < rhs.minor())
            return true;
        if (minor() > rhs.minor())
            return false;
        if (patch() < rhs.patch())
            return true;
        if (patch() > rhs.patch())
            return false;
        return tweak() < rhs.tweak();
    }

    bool VersionNumber::operator>(const VersionNumber &rhs) const {
        if (major() > rhs.major())
            return true;
        if (major() < rhs.major())
            return false;
        if (minor() > rhs.minor())
            return true;
        if (minor() < rhs.minor())
            return false;
        if (patch() > rhs.patch())
            return true;
        if (patch() < rhs.patch())
            return false;
        return tweak() > rhs.tweak();
    }

    bool VersionNumber::operator<=(const VersionNumber &rhs) const {
        return !(*this > rhs);
    }

    bool VersionNumber::operator>=(const VersionNumber &rhs) const {
        return !(*this < rhs);
    }

}

namespace std {

    size_t hash<stdc::VersionNumber>::operator()(const stdc::VersionNumber &key) const {
        size_t seed = typeid(key).hash_code();
        seed = stdc::hash(key.major(), seed);
        seed = stdc::hash(key.minor(), seed);
        seed = stdc::hash(key.patch(), seed);
        seed = stdc::hash(key.tweak(), seed);
        return seed;
    }

}