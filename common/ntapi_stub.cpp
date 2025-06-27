#include <cstdint>
#include "common/ntapi.h"

static u64 NtSetInformationFile_stub(HANDLE, PIO_STATUS_BLOCK, PVOID, ULONG, FILE_INFORMATION_CLASS) {
    return 0;
}

NtSetInformationFile_t NtSetInformationFile = NtSetInformationFile_stub; 