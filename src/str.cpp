#include "str.h"

#include "3rdparty/llvm/smallvector.h"

#ifdef _WIN32
#  include "osapi_win.h"
#endif

#include <cstring>

#include "path.h"

namespace stdc {

    /*！
        \namespace str
        \brief Namespace of string algorithms.
    */

    namespace str {

        /*!
            \fn std::string to_string(T &&t)

            Returns UTF-8 encoded string converted from supported classes.
        */

        /*!
            Joins all the string list's str into a single string.
        */
        std::string join(const std::vector<std::string> &v, const std::string_view &delimiter) {
            if (v.empty())
                return {};

            size_t length = 0;
            for (const auto &item : v) {
                length += item.size();
            }
            length += delimiter.size() * (v.size() - 1);

            std::string res;
            res.reserve(length);
            for (int i = 0; i < v.size() - 1; ++i) {
                res.append(v[i]);
                res.append(delimiter);
            }
            res.append(v.back());
            return res;
        }

        std::string join(const std::vector<std::string_view> &v,
                         const std::string_view &delimiter) {
            if (v.empty())
                return {};

            size_t length = 0;
            for (const auto &item : v) {
                length += item.size();
            }
            length += delimiter.size() * (v.size() - 1);

            std::string res;
            res.reserve(length);
            for (int i = 0; i < v.size() - 1; ++i) {
                res.append(v[i]);
                res.append(delimiter);
            }
            res.append(v.back());
            return res;
        }

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

        std::vector<std::string> split(std::string &&s, const std::string_view &delimiter) {
            std::vector<std::string> tokens;
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

        std::wstring conv<std::wstring>::from_utf8(const char *s, int size) {
            if (size < 0) {
                size = int(std::strlen(s));
            }
            if (size == 0) {
                return {};
            }
#ifdef _WIN32
            return win8bitToWide(std::string_view(s, size), CP_UTF8, MB_ERR_INVALID_CHARS);
#else
            return std::filesystem::path(std::string(s, size)).wstring();
#endif
        }

        std::string conv<std::wstring>::to_utf8(const wchar_t *s, int size) {
            if (size < 0) {
                size = int(std::wcslen(s));
            }
            if (size == 0) {
                return {};
            }
#ifdef _WIN32
            return winWideTo8bit(std::wstring_view(s, size), CP_UTF8, WC_ERR_INVALID_CHARS);
#else
            return std::filesystem::path(std::wstring(s, size)).string();
#endif
        }

#ifdef _WIN32

        std::wstring conv<std::wstring>::from_ansi(const char *s, int size) {
            if (size < 0) {
                size = int(std::strlen(s));
            }
            if (size == 0) {
                return {};
            }
            return win8bitToWide(std::string_view(s, size), CP_ACP, MB_ERR_INVALID_CHARS);
        }

        std::string conv<std::wstring>::to_ansi(const wchar_t *s, int size) {
            if (size < 0) {
                size = int(std::wcslen(s));
            }
            if (size == 0) {
                return {};
            }
            return winWideTo8bit(std::wstring_view(s, size), CP_ACP, WC_ERR_INVALID_CHARS);
        }
#endif

        std::string conv<std::filesystem::path>::normalize_separators(const std::string &utf8_path,
                                                                      bool native) {
            std::string res = utf8_path;
#if _WIN32
            if (native) {
                std::replace(res.begin(), res.end(), '/', '\\');
            } else {
                std::replace(res.begin(), res.end(), '\\', '/');
            }
#else
            (void) native;
            std::replace(res.begin(), res.end(), '\\', '/');
#endif
            return res;
        }

        /*!
            Replaces occurrences of \c %N in \a fmt string with the corresponding argument from
            \a args.
        */
        std::string format(const std::string_view &fmt, const std::vector<std::string> &args) {
            struct Part {
                const char *data;
                size_t size;
            };
            llvm::SmallVector<Part> parts;

            const auto &push_back = [&parts](const char *data, size_t size) {
                parts.push_back({data, size});
            };

            auto segment_start = fmt.data();
            auto format_end = fmt.data() + fmt.size();
            const auto &is_end = [format_end](const char *p) {
                return p == format_end; //
            };

            auto p = segment_start;
            while (p != format_end) {
                if (*p == '%' && !is_end(p + 1)) {
                    auto next = *(p + 1);
                    if (next == '%') { // Literal '%'
                        if (p > segment_start) {
                            push_back(segment_start, p - segment_start);
                        }
                        push_back("%", 1); // Add '%'
                        p += 2;
                        segment_start = p; // Skip "%%"
                        continue;
                    }
                    if (isdigit(next)) {
                        int index = next - '0';
                        auto q = p + 2;
                        while (!is_end(q) && isdigit(*q)) {
                            index = index * 10 + (*q - '0');
                            q++;
                        }
                        index--; // %1 -> index 0
                        if (index >= 0 && index < args.size()) {
                            if (p > segment_start) {
                                push_back(segment_start, p - segment_start);
                            }
                            push_back(args[index].data(), args[index].size());
                            segment_start = q;
                        } else {
                            // Invalid index, as literal
                        }
                        p = q;
                        continue;
                    }
                }
                p++;
            }
            if (p > segment_start) {
                push_back(segment_start, p - segment_start); // Add last part
            }

            size_t total_length = 0;
            for (int i = 0; i < parts.size(); i++) {
                total_length += parts[i].size;
            }

            // Construct result
            std::string res;
            res.resize(total_length);

            auto dest = res.data();
            for (int i = 0; i < parts.size(); i++) {
                memcpy(dest, parts[i].data, parts[i].size);
                dest += parts[i].size;
            }
            return res;
        }

        /*!
            \fn auto formatN(const std::string &format, Args &&...args)

            Replaces occurrences of \c %N in format string with the corresponding argument from
            \a args.
        */

        /*!
            Replace occurrences of \c ${VAR} in \a s with the corresponding variable from \a
            vars.
        */
        std::string parse_expr(const std::string_view &s,
                               const std::map<std::string, std::string> &vars) {
            std::string result;
            for (size_t i = 0; i < s.size();) {
                if (s[i] == '$' && i + 1 < s.size() && s[i + 1] == '{') {
                    size_t start = i + 2;
                    size_t j = start;

                    int braceCount = 1;
                    while (j < s.size() && braceCount > 0) {
                        if (s[j] == '{')
                            braceCount++;
                        if (s[j] == '}')
                            braceCount--;
                        j++;
                    }

                    if (braceCount == 0) {
                        std::string_view varName = s.substr(start, j - start - 1);
                        std::string innerValue = parse_expr(varName, vars);
                        auto it = vars.find(innerValue);
                        if (it != vars.end()) {
                            result += it->second;
                        }
                        i = j;
                    } else {
                        // Invalid expression
                        return {};
                    }
                } else {
                    result += s[i];
                    i++;
                }
            }

            // Replace "$$" with "$"
            std::string finalResult;
            for (size_t i = 0; i < result.size(); ++i) {
                if (result[i] == '$' && i + 1 < result.size() && result[i + 1] == '$') {
                    finalResult += '$';
                    ++i;
                } else {
                    finalResult += result[i];
                }
            }
            return finalResult;
        }

    }

}