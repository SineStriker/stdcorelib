#include "format.h"

#include <cstring>

#include "vla.h"

namespace stdc {

    /*!
        Replaces occurrences of \c %N in format string with the corresponding argument from \a args.
    */
    std::string formatText(const std::string_view &format, const std::vector<std::string> &args) {
        struct Part {
            const char *data;
            size_t size;
        };
        VarLengthArray<Part> parts(8);

        int parts_count = 0;
        const auto &push_back = [&parts, &parts_count](const char *data, size_t size) {
            if (parts_count == parts.size()) {
                parts.resize(parts.size() * 2);
            }
            parts[parts_count++] = {data, size};
        };

        auto segment_start = format.data();
        auto format_end = segment_start + format.size();
        auto p = segment_start;
        while (p != format_end) {
            if (*p == '%' && p + 1 != format_end) {
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
                    while (q != format_end && isdigit(*q)) {
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
        for (int i = 0; i < parts_count; i++) {
            total_length += parts[i].size;
        }

        // Construct result
        std::string res;
        res.resize(total_length);

        auto dest = res.data();
        for (int i = 0; i < parts_count; i++) {
            memcpy(dest, parts[i].data, parts[i].size);
            dest += parts[i].size;
        }
        return res;
    }

    /*!
        \fn auto formatTextN(const std::string &format, Args &&...args)

        Replaces occurrences of \c %N in format string with the corresponding argument from \a args.
    */

}