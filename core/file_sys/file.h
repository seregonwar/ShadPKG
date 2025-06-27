// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once
#include <filesystem>
#include <string>
#include "common/types.h"
#include "common/io_file.h" // Per Common::FS::FileAccessMode

namespace Core::FileSys {

class File {
public:
    int Open(const std::filesystem::path& path, Common::FS::FileAccessMode f_access);
    s64 Read(void* buf, size_t nbytes);
    s64 Pread(void* buf, size_t nbytes, s64 offset);
    s64 Write(const void* buf, size_t nbytes);
    s64 Pwrite(const void* buf, size_t nbytes, s64 offset);
    void SetSize(s64 size);
    void Flush();
    s64 Lseek(s64 offset, int whence);
    void Unlink();

private:
#ifdef _WIN64
    void* handle = nullptr; // HANDLE su Windows
#endif
};

} // namespace Core::FileSys