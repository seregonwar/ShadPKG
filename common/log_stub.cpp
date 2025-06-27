#include <fmt/core.h>
namespace Common {
namespace Log {
    enum class Class {};
    enum class Level {};
    void FmtLogMessageImpl(Class, Level, const char*, unsigned, const char*, const char*, const fmt::v11::basic_format_args<fmt::v11::context>&) {}
    void Stop() {}
}
} 