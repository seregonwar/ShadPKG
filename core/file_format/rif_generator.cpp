// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "rif_generator.h"
#include <fstream>
#include <regex>
#include <iomanip>
#include <sstream>
#include <cryptopp/md5.h>
#include "common/logging/log.h"

RIFGenerator::RIFGenerator() = default;
RIFGenerator::~RIFGenerator() = default;

bool RIFGenerator::ValidateContentID(const std::string& content_id) {
    // Formato: REGION-CUSAXXXXX_XX-PRODUCTCODE
    // Esempio: EP0001-CUSA12345_00-TESTGAMERETAIL01
    // Esempio: JP2551-CUSA16074_00-RASPBERRY000000
    std::regex pattern(R"(^[A-Z]{2}\d{4}-CUSA\d{5}_\d{2}-[A-Z0-9]{15,16}$)");
    return std::regex_match(content_id, pattern);
}

u32 RIFGenerator::GenerateTimestamp(const std::string& content_id) {
    // Genera un timestamp deterministico usando MD5 del Content ID
    CryptoPP::MD5 md5;
    u8 hash[CryptoPP::MD5::DIGESTSIZE];
    
    md5.Update(reinterpret_cast<const u8*>(content_id.data()), content_id.size());
    md5.Final(hash);
    
    // Usa i primi 4 byte dell'hash MD5 come timestamp
    u32 timestamp = 0;
    timestamp |= static_cast<u32>(hash[0]) << 24;
    timestamp |= static_cast<u32>(hash[1]) << 16;
    timestamp |= static_cast<u32>(hash[2]) << 8;
    timestamp |= static_cast<u32>(hash[3]);
    
    // Assicurati che sia nel range ragionevole (2013-2019)
    // Range osservato: 0x522ec370 - 0x5d96edf0
    const u32 min_timestamp = 0x522ec370; // ~2013
    const u32 max_timestamp = 0x5d96edf0; // ~2019
    
    // Normalizza il timestamp nel range
    timestamp = min_timestamp + (timestamp % (max_timestamp - min_timestamp));
    
    return timestamp;
}

std::vector<u8> RIFGenerator::GenerateRIFContent(const std::string& content_id) {
    std::vector<u8> rif_data(RIF_SIZE, 0);
    
    // Magic number "RIF\0"
    std::copy(RIF_MAGIC.begin(), RIF_MAGIC.end(), rif_data.begin() + MAGIC_OFFSET);
    
    // Version
    std::copy(RIF_VERSION.begin(), RIF_VERSION.end(), rif_data.begin() + VERSION_OFFSET);
    
    // Unknown field
    std::copy(RIF_UNKNOWN.begin(), RIF_UNKNOWN.end(), rif_data.begin() + UNKNOWN_OFFSET);
    
    // Padding (già inizializzato a 0)
    
    // Timestamp (big-endian)
    u32 timestamp = GenerateTimestamp(content_id);
    rif_data[TIMESTAMP_OFFSET + 0] = (timestamp >> 24) & 0xFF;
    rif_data[TIMESTAMP_OFFSET + 1] = (timestamp >> 16) & 0xFF;
    rif_data[TIMESTAMP_OFFSET + 2] = (timestamp >> 8) & 0xFF;
    rif_data[TIMESTAMP_OFFSET + 3] = timestamp & 0xFF;
    
    // Content ID
    if (content_id.size() <= (RIF_SIZE - CONTENT_ID_OFFSET)) {
        std::copy(content_id.begin(), content_id.end(), rif_data.begin() + CONTENT_ID_OFFSET);
    }
    
    return rif_data;
}

bool RIFGenerator::GenerateRIF(const std::string& content_id, const std::filesystem::path& output_path) {
    if (!ValidateContentID(content_id)) {
        LOG_ERROR(Lib_Kernel, "Invalid Content ID format: {}", content_id);
        return false;
    }
    
    try {
        // Genera il contenuto del file RIF
        auto rif_content = GenerateRIFContent(content_id);
        
        // Crea il nome del file RIF
        std::filesystem::path rif_filename = content_id + ".rif";
        std::filesystem::path full_path = output_path / rif_filename;
        
        // Crea la directory se non esiste
        std::filesystem::create_directories(output_path);
        
        // Scrivi il file RIF
        std::ofstream file(full_path, std::ios::binary);
        if (!file) {
            LOG_ERROR(Lib_Kernel, "Failed to create RIF file: {}", full_path.string());
            return false;
        }
        
        file.write(reinterpret_cast<const char*>(rif_content.data()), rif_content.size());
        file.close();
        
        if (file.fail()) {
            LOG_ERROR(Lib_Kernel, "Failed to write RIF file: {}", full_path.string());
            return false;
        }
        
        LOG_INFO(Lib_Kernel, "Generated RIF file: {} (size: {} bytes, timestamp: 0x{:08x})", 
                 full_path.string(), rif_content.size(), GenerateTimestamp(content_id));
        
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR(Lib_Kernel, "Exception while generating RIF: {}", e.what());
        return false;
    }
}

bool RIFGenerator::ValidateRIF(const std::filesystem::path& rif_path) {
    try {
        std::ifstream file(rif_path, std::ios::binary);
        if (!file) {
            return false;
        }
        
        // Verifica la dimensione del file
        file.seekg(0, std::ios::end);
        auto file_size = file.tellg();
        if (file_size != RIF_SIZE) {
            return false;
        }
        
        file.seekg(0, std::ios::beg);
        
        // Verifica il magic number
        std::array<u8, 4> magic;
        file.read(reinterpret_cast<char*>(magic.data()), magic.size());
        if (magic != RIF_MAGIC) {
            return false;
        }
        
        // Verifica la versione
        std::array<u8, 2> version;
        file.read(reinterpret_cast<char*>(version.data()), version.size());
        if (version != RIF_VERSION) {
            return false;
        }
        
        return true;
    } catch (const std::exception&) {
        return false;
    }
}