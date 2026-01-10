#include <cstdint>
#include <windows.h>

namespace Common { namespace FS {
class IOFile {
public:
    uint64_t GetFileMapping() { return 0; }
};
}}

extern "C" void* __imp_CreateFileMapping2 = nullptr; 