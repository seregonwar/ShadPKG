#include "rif_generator.h"
#include <cryptopp/md5.h>
#include <cryptopp/hex.h>
#include <fstream>
#include <filesystem>
#include <regex>
#include <iomanip>
#include <sstream>
#include "simple_log.h"

namespace RIF {

bool RIFGenerator::validateContentID(const std::string& contentID) {
    // PS4 Content ID format: XXXX-YYYYY_ZZ-WWWWWWWWWWWWWWWW
    // Where XXXX is region code, YYYYY is publisher code, ZZ is type, WWWW is title ID
    std::regex contentIDPattern(R"([A-Z]{4}-[A-Z0-9]{5}_[0-9]{2}-[A-Z0-9]{16})");
    return std::regex_match(contentID, contentIDPattern);
}

uint64_t RIFGenerator::generateDeterministicTimestamp(const std::string& contentID) {
    // Use MD5 hash of content ID to generate deterministic timestamp
    CryptoPP::MD5 hash;
    std::string digest;
    
    CryptoPP::StringSource(contentID, true,
        new CryptoPP::HashFilter(hash,
            new CryptoPP::StringSink(digest)
        )
    );
    
    // Convert first 8 bytes of hash to uint64_t
    uint64_t hashValue = 0;
    for (int i = 0; i < 8 && i < digest.length(); i++) {
        hashValue = (hashValue << 8) | static_cast<uint8_t>(digest[i]);
    }
    
    // Normalize to timestamp range (2013-2019)
    const uint64_t MIN_TIMESTAMP = 1356998400; // 2013-01-01
    const uint64_t MAX_TIMESTAMP = 1577836800; // 2020-01-01
    const uint64_t RANGE = MAX_TIMESTAMP - MIN_TIMESTAMP;
    
    return MIN_TIMESTAMP + (hashValue % RANGE);
}

std::vector<uint8_t> RIFGenerator::generateRIFContent(const std::string& contentID, uint64_t timestamp) {
    std::vector<uint8_t> rifData(RIF_SIZE, 0);
    
    // Set magic number
    rifData[0] = 0x52; // 'R'
    rifData[1] = 0x49; // 'I'
    rifData[2] = 0x46; // 'F'
    rifData[3] = 0x00;
    
    // Set version
    rifData[4] = 0x01;
    rifData[5] = 0x00;
    rifData[6] = 0x00;
    rifData[7] = 0x00;
    
    // Set unknown fields (observed pattern)
    rifData[8] = 0x00;
    rifData[9] = 0x01;
    rifData[10] = 0x00;
    rifData[11] = 0x00;
    
    // Set timestamp (little-endian)
    for (int i = 0; i < 8; i++) {
        rifData[TIMESTAMP_OFFSET + i] = (timestamp >> (i * 8)) & 0xFF;
    }
    
    // Set Content ID
    std::memcpy(&rifData[CONTENT_ID_OFFSET], contentID.c_str(), 
                std::min(contentID.length(), static_cast<size_t>(CONTENT_ID_SIZE)));
    
    return rifData;
}

bool RIFGenerator::generateRIF(const std::string& contentID, const std::string& outputPath) {
    if (!validateContentID(contentID)) {
        simple_log("Invalid Content ID format: " + contentID);
        return false;
    }
    
    // Generate deterministic timestamp
    uint64_t timestamp = generateDeterministicTimestamp(contentID);
    
    // Generate RIF content
    std::vector<uint8_t> rifData = generateRIFContent(contentID, timestamp);
    
    // Create output directory if it doesn't exist
    std::filesystem::path outputDir = std::filesystem::path(outputPath).parent_path();
    if (!outputDir.empty() && !std::filesystem::exists(outputDir)) {
        std::filesystem::create_directories(outputDir);
    }
    
    // Write RIF file
    std::ofstream rifFile(outputPath, std::ios::binary);
    if (!rifFile) {
        simple_log("Failed to create RIF file: " + outputPath);
        return false;
    }
    
    rifFile.write(reinterpret_cast<const char*>(rifData.data()), rifData.size());
    rifFile.close();
    
    simple_log("Generated RIF file: " + outputPath);
    return true;
}

bool RIFGenerator::validateRIF(const std::string& rifPath) {
    std::ifstream rifFile(rifPath, std::ios::binary);
    if (!rifFile) {
        simple_log("Failed to open RIF file: " + rifPath);
        return false;
    }
    
    // Check file size
    rifFile.seekg(0, std::ios::end);
    size_t fileSize = rifFile.tellg();
    if (fileSize != RIF_SIZE) {
        simple_log("Invalid RIF file size: " + std::to_string(fileSize) + " (expected " + std::to_string(RIF_SIZE) + ")");
        return false;
    }
    
    // Check magic number
    rifFile.seekg(0, std::ios::beg);
    char magic[4];
    rifFile.read(magic, 4);
    if (magic[0] != 'R' || magic[1] != 'I' || magic[2] != 'F' || magic[3] != 0x00) {
        simple_log("Invalid RIF magic number");
        return false;
    }
    
    simple_log("RIF file validation successful: " + rifPath);
    return true;
}

} // namespace RIF