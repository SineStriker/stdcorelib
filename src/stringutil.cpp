#include "stringutil.h"

namespace stdc {

    /*!
        Splits the string into substring view list.
    */
    std::vector<std::string_view> split(const std::string_view &s,
                                        const std::string_view &delimiter) {
        std::vector<std::string_view> tokens;
        std::string::size_type start = 0;
        std::string::size_type end = s.find(delimiter);
        while (end != std::string::npos) {
            tokens.push_back(s.substr(start, end - start));
            start = end + delimiter.size();
            end = s.find(delimiter, start);
        }
        tokens.push_back(s.substr(start));
        return tokens;
    }

    /*!
        Joins all the string list's strings into a single string.
    */
    std::string join(const std::vector<std::string> &v, const std::string_view &delimiter) {
        if (v.empty())
            return {};

        std::string res;
        for (int i = 0; i < v.size() - 1; ++i) {
            res.append(v[i]);
            res.append(delimiter);
        }
        res.append(v.back());
        return res;
    }

}