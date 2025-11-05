#ifndef STDCORELIB_LOGGING_H
#define STDCORELIB_LOGGING_H

#include <stdcorelib/str.h>

namespace stdc {

    class LogContext {
    public:
        inline LogContext() noexcept = default;
        inline LogContext(const char *fileName, int lineNumber, const char *functionName,
                          const char *categoryName) noexcept
            : line(lineNumber), file(fileName), function(functionName), category(categoryName) {
        }

        int line = 0;
        const char *file = nullptr;
        const char *function = nullptr;
        const char *category = nullptr;
    };

    class STDCORELIB_EXPORT Logger {
    public:
        enum Level {
            Trace = 1,
            Debug,
            Success,
            Information,
            Warning,
            Critical,
            Fatal,
        };

        inline Logger(LogContext context) : _context(std::move(context)) {
        }

        inline Logger(const char *file, int line, const char *function, const char *category)
            : _context(file, line, function, category) {
        }

        template <class... Args>
        inline void trace(const std::string_view &format, Args &&...args) {
            print(Trace, stdc::formatN(format, std::forward<Args>(args)...));
        }

        template <class... Args>
        inline void debug(const std::string_view &format, Args &&...args) {
            print(Debug, stdc::formatN(format, std::forward<Args>(args)...));
        }

        template <class... Args>
        inline void success(const std::string_view &format, Args &&...args) {
            print(Success, stdc::formatN(format, std::forward<Args>(args)...));
        }

        template <class... Args>
        inline void info(const std::string_view &format, Args &&...args) {
            print(Information, stdc::formatN(format, std::forward<Args>(args)...));
        }

        template <class... Args>
        inline void warning(const std::string_view &format, Args &&...args) {
            print(Warning, stdc::formatN(format, std::forward<Args>(args)...));
        }

        template <class... Args>
        inline void critical(const std::string_view &format, Args &&...args) {
            print(Critical, stdc::formatN(format, std::forward<Args>(args)...));
        }

        template <class... Args>
        [[noreturn]] inline void fatal(const std::string_view &format, Args &&...args) {
            print(Critical, stdc::formatN(format, std::forward<Args>(args)...));
            abort();
        }

        template <class... Args>
        inline void log(int level, const std::string_view &format, Args &&...args) {
            print(level, stdc::formatN(format, std::forward<Args>(args)...));
        }

        void print(int level, const std::string_view &message);

        void printf(int level, const char *fmt, ...);

        [[noreturn]] static void abort();

    public:
        using LogCallback = void (*)(int, const LogContext &, const std::string_view &);

        static LogCallback logCallback();
        static void setLogCallback(LogCallback callback);

    protected:
        LogContext _context;
    };

    /// Yet another logging category implementation of Qt QLoggingCategory.
    class STDCORELIB_EXPORT LogCategory {
    public:
        explicit LogCategory(const char *name);
        ~LogCategory();

        inline const char *name() const {
            return _name;
        }
        inline bool isLevelEnabled(int level) const {
            return levelEnabled[level];
        }
        inline void setLevelEnabled(int level, bool enabled) {
            levelEnabled[level] = enabled;
        }

        using LogCategoryFilter = void (*)(LogCategory *);

        static LogCategoryFilter logFilter();
        static void setLogFilter(LogCategoryFilter filter);

        static std::string filterRules();
        void setFilterRules(std::string rules);

        static LogCategory &defaultCategory();

        template <int Level, class... Args>
        void log(const char *fileName, int lineNumber, const char *functionName,
                 const std::string_view &format, Args &&...args) const {
            if (!isLevelEnabled(Level)) {
                return;
            }
            Logger(fileName, lineNumber, functionName, _name)
                .log(Level, format, std::forward<Args>(args)...);
            if constexpr (Level == stdc::Logger::Fatal) {
                Logger::abort();
            }
        }

        template <int Level, class... Args>
        void logf(const char *fileName, int lineNumber, const char *functionName, const char *fmt,
                  Args &&...args) const {
            if (!isLevelEnabled(Level)) {
                return;
            }
            Logger(fileName, lineNumber, functionName, _name)
                .printf(Level, fmt, std::forward<Args>(args)...);
            if constexpr (Level == stdc::Logger::Fatal) {
                Logger::abort();
            }
        }

        inline const LogCategory &_stdcGetLogCategory() const {
            return *this;
        }

    protected:
        const char *_name;

        union {
            bool levelEnabled[8];
            uint64_t enabled;
        };
    };

}

static inline const stdc::LogCategory &_stdcGetLogCategory() {
    return stdc::LogCategory::defaultCategory();
}

/*!
    \macro stdcDebug
    \brief Logs a debug message to a log category.
    \code
        // User category
        stdc::LogCategory lc("test");
        lc.setLevelEnabled(stdc::Logger::Debug, true);
        lc.stdcDebug("This is a debug message");
        lc.stdcDebug("This is a debug message with arg: %1", 42);
        lc.stdcDebugF("This is a debug message with arg: %d", 42);

        // Default category
        stdcDebug("This is a debug message");
        stdcDebug("This is a debug message with arg: %1", 42);
        stdcDebug("This is a debug message with arg: %d", 42);
    \endcode
*/

#define stdcLog(LEVEL, ...)                                                                        \
  _stdcGetLogCategory().log<stdc::Logger::LEVEL>(__FILE__, __LINE__, __FUNCTION__, __VA_ARGS__)
#define stdcTrace(...)    stdcLog(Trace, __VA_ARGS__)
#define stdcDebug(...)    stdcLog(Debug, __VA_ARGS__)
#define stdcSuccess(...)  stdcLog(Success, __VA_ARGS__)
#define stdcInfo(...)     stdcLog(Information, __VA_ARGS__)
#define stdcWarning(...)  stdcLog(Warning, __VA_ARGS__)
#define stdcCritical(...) stdcLog(Critical, __VA_ARGS__)
#define stdcFatal(...)    stdcLog(Fatal, __VA_ARGS__)

#define stdcLogF(LEVEL, ...)                                                                       \
  _stdcGetLogCategory().logf<stdc::Logger::LEVEL>(__FILE__, __LINE__, __FUNCTION__, __VA_ARGS__)
#define stdcTraceF(...)    stdcLogF(Trace, __VA_ARGS__)
#define stdcDebugF(...)    stdcLogF(Debug, __VA_ARGS__)
#define stdcSuccessF(...)  stdcLogF(Success, __VA_ARGS__)
#define stdcInfoF(...)     stdcLogF(Information, __VA_ARGS__)
#define stdcWarningF(...)  stdcLogF(Warning, __VA_ARGS__)
#define stdcCriticalF(...) stdcLogF(Critical, __VA_ARGS__)
#define stdcFatalF(...)    stdcLogF(Fatal, __VA_ARGS__)

#endif // STDCORELIB_LOGGING_H
