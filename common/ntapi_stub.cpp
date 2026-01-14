#include <cstdint>
#include "common/ntapi.h"
#include "common/shadpkg_types.h"

#ifdef _WIN32
static u64 NtSetInformationFile_stub(HANDLE, PIO_STATUS_BLOCK, PVOID, ULONG, FILE_INFORMATION_CLASS) {
    return 0;
}

NtSetInformationFile_t NtSetInformationFile = NtSetInformationFile_stub;
#endif
