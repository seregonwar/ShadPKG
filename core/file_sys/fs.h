// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <atomic>
#include <mutex>
#include <string>
#include <vector>
#include <functional>
#include "common/io_file.h"
#include "common/logging/formatter.h"
#include "core/devices/base_device.h"

namespace Core::FileSys {

struct DirEntry {
    std::string name;
    bool isFile;
};

enum class FileType {
    Regular, // standard file
    Directory,
    Device,
};

struct File {
    std::atomic_bool is_opened{};
    std::atomic<FileType> type{FileType::Regular};
    std::filesystem::path m_host_name;
    std::string m_guest_name;
    Common::FS::IOFile f;
    std::vector<DirEntry> dirents;
    u32 dirents_index;
    std::mutex m_mutex;
    std::shared_ptr<Devices::BaseDevice> device; // only valid for type == Device
};

class HandleTable {
public:
    HandleTable() = default;
    virtual ~HandleTable() = default;

    int CreateHandle();
    void DeleteHandle(int d);
    File* GetFile(int d);
    File* GetFile(const std::filesystem::path& host_name);
    int GetFileDescriptor(File* file);

    void CreateStdHandles();

private:
    std::vector<File*> m_files;
    std::mutex m_mutex;
};

} // namespace Core::FileSys
