// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>
#include "common/config.h"
#include "common/string_util.h"
#include "core/devices/logger.h"
#include "core/devices/nop_device.h"
#include "core/file_sys/fs.h"

namespace Core::FileSys {

std::string RemoveTrailingSlashes(const std::string& path) {
    // Remove trailing slashes to make comparisons simpler.
    std::string path_sanitized = path;
    while (path_sanitized.ends_with("/")) {
        path_sanitized.pop_back();
    }
    return path_sanitized;
}

int HandleTable::CreateHandle() {
    std::scoped_lock lock{m_mutex};

    auto* file = new File{};
    file->is_opened = false;

    int existingFilesNum = m_files.size();

    for (int index = 0; index < existingFilesNum; index++) {
        if (m_files.at(index) == nullptr) {
            m_files[index] = file;
            return index;
        }
    }

    m_files.push_back(file);
    return m_files.size() - 1;
}

void HandleTable::DeleteHandle(int d) {
    std::scoped_lock lock{m_mutex};
    delete m_files.at(d);
    m_files[d] = nullptr;
}

File* HandleTable::GetFile(int d) {
    std::scoped_lock lock{m_mutex};
    if (d < 0 || d >= m_files.size()) {
        return nullptr;
    }
    return m_files.at(d);
}

File* HandleTable::GetFile(const std::filesystem::path& host_name) {
    for (auto* file : m_files) {
        if (file != nullptr && file->m_host_name == host_name) {
            return file;
        }
    }
    return nullptr;
}

void HandleTable::CreateStdHandles() {
    auto setup = [this](const char* path, auto* device) {
        int fd = CreateHandle();
        auto* file = GetFile(fd);
        file->is_opened = true;
        file->type = FileType::Device;
        file->m_guest_name = path;
        file->device =
            std::shared_ptr<Devices::BaseDevice>{reinterpret_cast<Devices::BaseDevice*>(device)};
    };
    // order matters
    setup("/dev/stdin", new Devices::Logger("stdin", false));   // stdin
    setup("/dev/stdout", new Devices::Logger("stdout", false)); // stdout
    setup("/dev/stderr", new Devices::Logger("stderr", true));  // stderr
}

int HandleTable::GetFileDescriptor(File* file) {
    std::scoped_lock lock{m_mutex};
    auto it = std::find(m_files.begin(), m_files.end(), file);

    if (it != m_files.end()) {
        return std::distance(m_files.begin(), it);
    }
    return 0;
}

} // namespace Core::FileSys
