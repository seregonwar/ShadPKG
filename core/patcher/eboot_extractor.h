#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <filesystem>

namespace ShadPKG::Patcher {

// Lightweight eboot.bin extractor from PS4 PKG
// Extracts only the eboot.bin without processing the entire PKG
class EbootExtractor {
public:
    EbootExtractor() = default;
    ~EbootExtractor() = default;

    // Extract eboot.bin from PKG to output path
    bool extractEboot(const std::filesystem::path& pkgPath, 
                      const std::filesystem::path& outputPath);
    
    // Get the raw eboot data after extraction
    const std::vector<uint8_t>& getEbootData() const { return ebootData_; }
    
    // Get game info from PKG
    std::string getTitleId() const { return titleId_; }
    std::string getContentId() const { return contentId_; }

private:
    std::vector<uint8_t> ebootData_;
    std::string titleId_;
    std::string contentId_;
    
    // PKG entry IDs
    static constexpr uint32_t PKG_ENTRY_ID_EBOOT = 0x1001;  // eboot.bin
    static constexpr uint32_t PKG_ENTRY_ID_PFS = 0x1200;   // PFS image
};

} // namespace ShadPKG::Patcher
