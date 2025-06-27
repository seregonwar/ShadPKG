#pragma once
#include <string>

namespace Core {
namespace Devices {
class Logger {
public:
    Logger(std::basic_string<char, std::char_traits<char>, std::allocator<char>>, bool);
};
}
} 