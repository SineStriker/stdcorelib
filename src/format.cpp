#include "format.h"

namespace cpputils {

    /*!
        \fn std::string anyToString(T &&t)

        Returns UTF-8 encoded string converted from supported classes.
    */

    /*!
        Replaces occurrences of \c %N in format string with the corresponding argument from \a args.
    */
    std::string formatText(const std::string &format, const std::vector<std::string> &args) {
        std::string result = format;
        for (size_t i = 0; i < args.size(); i++) {
            std::string placeholder = "%" + std::to_string(i + 1);
            size_t pos = result.find(placeholder);
            while (pos != std::string::npos) {
                result.replace(pos, placeholder.length(), args[i]);
                pos = result.find(placeholder, pos + args[i].size());
            }
        }
        return result;
    }

    /*!
        \fn auto formatTextN(const std::string &format, Args &&...args)

        Replaces occurrences of \c %N in format string with the corresponding argument from \a args.
    */

}