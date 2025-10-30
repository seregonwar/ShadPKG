// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <filesystem>
#include <string>
#include <vector>
#include "common/types.h"

class RIFGenerator {
public:
    RIFGenerator();
    ~RIFGenerator();

    // Valida il formato del Content ID
    static bool ValidateContentID(const std::string& content_id);

    // Genera un file RIF per un Content ID specifico
    bool GenerateRIF(const std::string& content_id, const std::filesystem::path& output_path);

    // Genera un timestamp deterministico basato sul Content ID
    static u32 GenerateTimestamp(const std::string& content_id);

    // Genera il contenuto del file RIF
    static std::vector<u8> GenerateRIFContent(const std::string& content_id);

    // Verifica se un file RIF è valido
    static bool ValidateRIF(const std::filesystem::path& rif_path);

private:
    // Costanti per la struttura RIF
    static constexpr u32 RIF_SIZE = 1024;
    static constexpr std::array<u8, 4> RIF_MAGIC = {0x52, 0x49, 0x46, 0x00}; // "RIF\0"
    static constexpr std::array<u8, 2> RIF_VERSION = {0x00, 0x01};
    static constexpr std::array<u8, 2> RIF_UNKNOWN = {0xFF, 0xFF};
    
    // Offset per i vari campi nel file RIF
    static constexpr u32 MAGIC_OFFSET = 0x00;
    static constexpr u32 VERSION_OFFSET = 0x04;
    static constexpr u32 UNKNOWN_OFFSET = 0x06;
    static constexpr u32 PADDING_OFFSET = 0x08;
    static constexpr u32 TIMESTAMP_OFFSET = 0x10;
    static constexpr u32 CONTENT_ID_OFFSET = 0x40;
};