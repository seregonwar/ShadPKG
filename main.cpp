// SPDX-FileCopyrightText: Copyright 2025 shadPKG
// SPDX-License-Identifier: GPL-2.0-or-later

#include "common/logging/backend.h"
#include "common/logging/log.h"
#include "core/decompiler/DecompilerContext.h"
#include "core/decompiler/analysis/MemberAccessAnalysis.h"
#include "core/file_format/iso_extractor.h"
#include "core/file_format/pkg.h"
#include "core/file_format/psf.h"
#include "core/file_format/rif_generator.h"
#include "core/patcher/elf_reconstruct.h"
#include "core/patcher/psn_bypass.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

void showUsage(const char *program_name) {
  std::cout << R"(

         __              ______  __ ________
   _____/ /_  ____ _____/ / __ \/ //_/ ____/
  / ___/ __ \/ __ `/ __  / /_/ / ,< / / __  
 (__  ) / / / /_/ / /_/ / ____/ /| / /_/ /  
/____/_/ /_/\__,_/\__,_/_/   /_/ |_\____/   
                                                                                        
https://github.com/seregonwar/shadPKG
---------------------------------------------

Usage:
  )" << program_name
            << R"( extract [options] <file.pkg> [output_dir] [file.rif]
  )" << program_name
            << R"( generate-rif <content_id> [output_dir]
  )" << program_name
            << R"( validate-rif <file.rif>
  )" << program_name
            << R"( pfs-info [options] <file.pkg>
  )" << program_name
            << R"( sfo-info [options] <file.pkg>
  )" << program_name
            << R"( patch [options] <eboot.bin>
  )" << program_name
            << R"( patch-pkg [options] <minecraft.pkg> [output.pkg]

Commands:
  extract       - Extracts and decrypts a PKG file and optionally decompiles it.
  generate-rif  - Generates a RIF file for the specified Content ID
  validate-rif  - Validates an existing RIF file
  pfs-info      - Scans the PKG and shows the PFS structure without extracting files
  sfo-info      - Displays param.sfo parameters from a PKG file
  patch         - Apply PSN bypass patches to eboot.bin (Minecraft, etc.)
  patch-pkg     - Complete PKG patcher: extract, scan, patch, and create installation-ready PKG

General Options:
  -h, --help    - Shows this help

Options for extract:
  -i, --input <file>    - Input PKG file
  -o, --output <dir>    - Output directory (fallback: creates subdirectory with TitleID)
  -r, --rif <file>      - Path to the RIF file to use during decryption
  
  --export-project <dir> - Decompile and export as C++ project to <dir>
  --list-functions       - List all analyzed functions (Addr, Size, Name)
  --decompile <hex_addr> - Decompile a single function to stdout

Options for pfs-info/sfo-info:
  --json            - Prints output in JSON format
  -v, --verbose     - Shows additional details (types, sizes)
  -q, --query KEY   - Query a single parameter value

Options for patch:
  -i, --input <file>     - Input eboot.bin file
  -o, --output <file>    - Output patched eboot.bin (default: eboot_patched.bin)
  --game <id>            - Game ID (e.g., CUSA00265 for Minecraft EU)
  --version <ver>        - Game version (e.g., 01.00)
  --scan                 - Scan for PSN functions without patching
  --gen-options          - Generate Minecraft options.txt for PSN bypass
  --elf                  - Convert output to valid ELF64 (for emulators like shadPS4)

Options for patch-pkg:
  -i, --input <file>     - Input PKG file
  -o, --output <file>    - Output patched PKG (default: <input>_patched.pkg)
  --game <id>            - Game ID (default: CUSA00265 for Minecraft)
  --extract-only         - Only extract and prepare, don't build final PKG

Examples:
  )" << program_name
            << R"( extract game.pkg ./output
  )" << program_name
            << R"( extract --input game.pkg --export-project ./MyDecompiledGame
  )" << program_name
            << R"( extract game.pkg -o ./out --list-functions
)";
}

// Helper to read binary file
std::vector<uint8_t> readFile(const std::filesystem::path &path) {
  std::ifstream file(path, std::ios::binary | std::ios::ate);
  if (!file)
    return {};
  std::streamsize size = file.tellg();
  file.seekg(0, std::ios::beg);
  std::vector<uint8_t> buffer(size);
  if (file.read((char *)buffer.data(), size))
    return buffer;
  return {};
}

// Helper to detect file type (PKG vs ISO vs ELF)
enum class FileType { Unknown, PKG, ISO_PS2, ISO_PS3, ELF };

FileType detectFileType(const std::filesystem::path &path) {
  std::ifstream file(path, std::ios::binary);
  if (!file.is_open())
    return FileType::Unknown;

  // Check ELF magic (0x7F 0x45 0x4C 0x46 = "\x7FELF")
  char magic[4];
  file.read(magic, 4);
  if (magic[0] == 0x7F && magic[1] == 0x45 && magic[2] == 0x4C &&
      magic[3] == 0x46)
    return FileType::ELF;

  // Check PKG magic (0x7F434E54 = ".CNT" in big-endian)
  if (magic[0] == 0x7F && magic[1] == 0x43 && magic[2] == 0x4E &&
      magic[3] == 0x54)
    return FileType::PKG;

  // Check ISO 9660 signature at offset 0x8001
  file.seekg(0x8001);
  char sig[5] = {0};
  file.read(sig, 5);
  if (std::strncmp(sig, "CD001", 5) == 0) {
    // Check if PS2 or PS3
    file.seekg(0);
    std::vector<char> buffer(2048);
    file.read(buffer.data(), buffer.size());
    std::string content(buffer.begin(), buffer.end());
    if (content.find("BOOT2") != std::string::npos)
      return FileType::ISO_PS3;
    return FileType::ISO_PS2;
  }

  // Check UDF signature
  file.seekg(0x8000);
  file.read(sig, 5);
  if (std::strncmp(sig, "BEA01", 5) == 0)
    return FileType::ISO_PS3;

  return FileType::Unknown;
}

int main(int argc, char *argv[]) {
  Common::Log::Initialize("pkg_extraction.log");
  Common::Log::SetColorConsoleBackendEnabled(true);
  LOG_INFO(Common, "[START] Starting PKG extractor with RIF generator");

  try {
    if (argc < 2) {
      showUsage(argv[0]);
      return 1;
    }

    std::string command = argv[1];

    // Global Help
    if (command == "-h" || command == "--help" || command == "-help" ||
        command == "-?") {
      showUsage(argv[0]);
      return 0;
    }

    // Command to generate RIF file
    if (command == "generate-rif") {
      if (argc < 3) {
        std::cerr << "Error: Content ID required for generate-rif\n";
        showUsage(argv[0]);
        return 1;
      }

      std::string content_id = argv[2];
      std::filesystem::path output_dir = (argc >= 4) ? argv[3] : ".";

      RIFGenerator generator;
      if (generator.GenerateRIF(content_id, output_dir)) {
        std::cout << "RIF file successfully generated for: " << content_id
                  << std::endl;
        return 0;
      } else {
        std::cerr << "Error generating RIF file" << std::endl;
        return 1;
      }
    }

    // Command to validate RIF file
    if (command == "validate-rif") {
      if (argc < 3) {
        std::cerr << "Error: RIF file required for validate-rif\n";
        showUsage(argv[0]);
        return 1;
      }

      std::filesystem::path rif_path = argv[2];
      if (RIFGenerator::ValidateRIF(rif_path)) {
        std::cout << "Valid RIF file: " << rif_path << std::endl;
        return 0;
      } else {
        std::cerr << "Invalid RIF file: " << rif_path << std::endl;
        return 1;
      }
    }

    // Lightweight command: pfs-info
    if (command == "pfs-info") {
      bool as_json = false;
      std::filesystem::path pkg_path;
      for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help" || arg == "-help" || arg == "-?") {
          showUsage(argv[0]);
          return 0;
        } else if (arg == "--json") {
          as_json = true;
        } else if (!arg.empty() && arg[0] == '-') {
          std::cerr << "Unrecognized option: " << arg << std::endl;
          showUsage(argv[0]);
          return 1;
        } else if (pkg_path.empty()) {
          pkg_path = arg;
        } else {
          std::cerr << "Excess arguments for pfs-info" << std::endl;
          return 1;
        }
      }

      if (pkg_path.empty()) {
        std::cerr << "Error: PKG file must be specified" << std::endl;
        showUsage(argv[0]);
        return 1;
      }

      std::string failreason;
      PKG pkg;
      if (!pkg.Scan(pkg_path, failreason)) {
        std::cerr << "Error in pfs-info: " << failreason << std::endl;
        return 1;
      }

      // Header/summary
      auto header = pkg.GetPkgHeader();
      auto title_id = pkg.GetTitleID();
      auto flags = pkg.GetPkgFlags();
      auto entries = pkg.GetEntriesInfo();
      size_t n_files = 0, n_dirs = 0;
      for (const auto &e : entries) {
        if (e.type == PFS_FILE)
          n_files++;
        else if (e.type == PFS_DIR)
          n_dirs++;
      }

      if (as_json) {
        // Simple JSON output
        std::cout << "{\n";
        std::cout << "  \"title_id\": \"" << std::string(title_id) << "\",\n";
        std::cout << "  \"flags\": \"" << flags << "\",\n";
        std::cout << "  \"pkg_size\": " << pkg.GetPkgSize() << ",\n";
        std::cout << "  \"entries\": [\n";
        for (size_t i = 0; i < entries.size(); ++i) {
          const auto &e = entries[i];
          std::cout << "    {\n";
          std::cout << "      \"name\": \"" << e.name << "\",\n";
          std::cout << "      \"inode\": " << e.inode << ",\n";
          std::cout << "      \"type\": " << e.type << ",\n";
          std::cout << "      \"path\": \"" << e.path << "\",\n";
          std::cout << "      \"size\": " << e.size << ",\n";
          std::cout << "      \"blocks\": " << e.blocks << ",\n";
          std::cout << "      \"loc\": " << e.loc << "\n";
          std::cout << "    }" << (i + 1 < entries.size() ? "," : "") << "\n";
        }
        std::cout << "  ],\n";
        std::cout << "  \"summary\": { \"files\": " << n_files
                  << ", \"dirs\": " << n_dirs << " }\n";
        std::cout << "}\n";
      } else {
        // Human readable output
        std::cout << "\n--- PFS Info ---\n";
        std::cout << "TitleID: " << title_id << "\n";
        std::cout << "Flags: " << flags << "\n";
        std::cout << "PKG Size: " << pkg.GetPkgSize() << "\n";
        std::cout << "Dirs: " << n_dirs << ", Files: " << n_files << "\n\n";
        for (const auto &e : entries) {
          std::cout << (e.type == PFS_DIR
                            ? "[D] "
                            : (e.type == PFS_FILE ? "[F] " : "[?] "))
                    << e.path << (e.path.empty() ? e.name : std::string())
                    << (e.type == PFS_FILE
                            ? (" (size=" + std::to_string(e.size) + ")")
                            : "")
                    << "\n";
        }
      }
      return 0;
    }

    // ╔═══════════════════════════════════════════════════════════════════════╗
    // ║  sfo-info: Display param.sfo parameters from PKG                      ║
    // ╚═══════════════════════════════════════════════════════════════════════╝
    if (command == "sfo-info") {
      bool as_json = false;
      bool verbose = false;
      std::string query_key;
      std::filesystem::path pkg_path;

      for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help") {
          showUsage(argv[0]);
          return 0;
        } else if (arg == "--json") {
          as_json = true;
        } else if (arg == "-v" || arg == "--verbose") {
          verbose = true;
        } else if (arg == "-q" || arg == "--query") {
          if (i + 1 >= argc) {
            std::cerr << "Error: --query requires a parameter name"
                      << std::endl;
            return 1;
          }
          query_key = argv[++i];
        } else if (!arg.empty() && arg[0] == '-') {
          std::cerr << "Unrecognized option: " << arg << std::endl;
          showUsage(argv[0]);
          return 1;
        } else if (pkg_path.empty()) {
          pkg_path = arg;
        } else {
          std::cerr << "Excess arguments for sfo-info" << std::endl;
          return 1;
        }
      }

      if (pkg_path.empty()) {
        std::cerr << "Error: PKG file must be specified" << std::endl;
        showUsage(argv[0]);
        return 1;
      }

      // Open PKG to get the SFO data
      std::string failreason;
      PKG pkg;
      if (!pkg.Open(pkg_path, failreason)) {
        std::cerr << "Error opening PKG: " << failreason << std::endl;
        return 1;
      }

      // Parse SFO from PKG's internal buffer
      PSF psf;
      if (pkg.sfo.empty()) {
        std::cerr << "Error: param.sfo not found in PKG (entry 0x1000 missing "
                     "or lookup failed)"
                  << std::endl;
        return 1;
      }
      if (!psf.Open(pkg.sfo)) {
        std::cerr << "Error: Malformed param.sfo data (Open failed)"
                  << std::endl;
        return 1;
      }

      const auto &entries = psf.GetEntries();

      // Query mode: return just one value
      if (!query_key.empty()) {
        // Try string first
        if (auto str = psf.GetString(query_key); str.has_value()) {
          std::cout << *str << std::endl;
          return 0;
        }
        // Try integer
        if (auto val = psf.GetInteger(query_key); val.has_value()) {
          std::cout << *val << std::endl;
          return 0;
        }
        // Try binary (output as hex)
        if (auto bin = psf.GetBinary(query_key); bin.has_value()) {
          for (const auto &b : *bin) {
            printf("%02x", b);
          }
          std::cout << std::endl;
          return 0;
        }
        std::cerr << "Parameter not found: " << query_key << std::endl;
        return 1;
      }

      // JSON output
      if (as_json) {
        std::cout << "{\n";
        std::cout << "  \"title_id\": \"" << std::string(pkg.GetTitleID())
                  << "\",\n";
        std::cout << "  \"parameters\": {\n";
        size_t idx = 0;
        for (const auto &entry : entries) {
          std::cout << "    \"" << entry.key << "\": ";
          switch (entry.param_fmt) {
          case PSFEntryFmt::Text:
            if (auto v = psf.GetString(entry.key); v.has_value()) {
              std::cout << "\"" << *v << "\"";
            }
            break;
          case PSFEntryFmt::Integer:
            if (auto v = psf.GetInteger(entry.key); v.has_value()) {
              std::cout << *v;
            }
            break;
          case PSFEntryFmt::Binary:
            if (auto v = psf.GetBinary(entry.key); v.has_value()) {
              std::cout << "\"0x";
              for (const auto &b : *v)
                printf("%02x", b);
              std::cout << "\"";
            }
            break;
          }
          std::cout << (++idx < entries.size() ? "," : "") << "\n";
        }
        std::cout << "  }\n}\n";
        return 0;
      }

      // Human readable output
      std::cout << "\n--- SFO Info ---\n";
      std::cout << "TitleID: " << pkg.GetTitleID() << "\n";
      if (verbose) {
        std::cout << "Parameters: " << entries.size() << "\n";
      }
      std::cout << "\n";

      for (const auto &entry : entries) {
        switch (entry.param_fmt) {
        case PSFEntryFmt::Text:
          if (auto v = psf.GetString(entry.key); v.has_value()) {
            if (verbose) {
              std::cout << entry.key << "=\"" << *v
                        << "\" (str, max=" << entry.max_len << ")\n";
            } else {
              std::cout << entry.key << "=" << *v << "\n";
            }
          }
          break;
        case PSFEntryFmt::Integer:
          if (auto v = psf.GetInteger(entry.key); v.has_value()) {
            if (verbose) {
              std::cout << entry.key << "=" << *v << " (0x" << std::hex << *v
                        << std::dec << ", int)\n";
            } else {
              std::cout << entry.key << "=" << *v << "\n";
            }
          }
          break;
        case PSFEntryFmt::Binary:
          if (auto v = psf.GetBinary(entry.key); v.has_value()) {
            std::cout << entry.key << "=0x";
            for (const auto &b : *v)
              printf("%02x", b);
            if (verbose) {
              std::cout << " (binary, " << v->size() << " bytes)";
            }
            std::cout << "\n";
          }
          break;
        }
      }
      return 0;
    }

    // Command to extract PKG
    if (command == "extract") {
      std::cerr << "DEBUG: Entered extract block" << std::endl;
      std::cerr.flush();
      // Flexible parsing: supports both positional (legacy) and options
      std::filesystem::path pkg_path;
      std::filesystem::path out_dir = "."; // default
      std::filesystem::path rif_path;
      bool use_rif = false;

      // Decompiler Flags
      bool export_project = false;
      std::filesystem::path export_dir;
      bool list_functions = false;
      bool decompile_single = false;
      uint64_t decompile_addr = 0;
      std::filesystem::path load_db_path;

      for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help" || arg == "-help" || arg == "-?") {
          showUsage(argv[0]);
          return 0;
        } else if (arg == "-o" || arg == "--output") {
          if (i + 1 >= argc) {
            std::cerr << "Error: missing value for --output" << std::endl;
            return 1;
          }
          out_dir = argv[++i];
        } else if (arg == "-i" || arg == "--input") {
          if (i + 1 >= argc) {
            std::cerr << "Error: missing value for --input" << std::endl;
            return 1;
          }
          pkg_path = argv[++i];
        } else if (arg == "-r" || arg == "--rif") {
          if (i + 1 >= argc) {
            std::cerr << "Error: missing value for --rif" << std::endl;
            return 1;
          }
          rif_path = argv[++i];
          use_rif = true;
        } else if (arg == "--export-project") {
          if (i + 1 >= argc) {
            std::cerr << "Error: missing value for --export-project"
                      << std::endl;
            return 1;
          }
          export_project = true;
          export_dir = argv[++i];
        } else if (arg == "--list-functions") {
          list_functions = true;
        } else if (arg == "--decompile") {
          if (i + 1 >= argc) {
            std::cerr << "Error: missing address for --decompile" << std::endl;
            return 1;
          }
          decompile_single = true;
          std::string addrStr = argv[++i];
          try {
            decompile_addr = std::stoull(addrStr, nullptr, 16);
          } catch (...) {
            std::cerr << "Error: invalid hex address: " << addrStr << std::endl;
            return 1;
          }
        } else if (arg == "--load-db") {
          if (i + 1 >= argc) {
            std::cerr << "Error: missing value for --load-db" << std::endl;
            return 1;
          }
          load_db_path = argv[++i];
        } else if (!arg.empty() && arg[0] == '-') {
          std::cerr << "Unrecognized option: " << arg << std::endl;
          showUsage(argv[0]);
          return 1;
        } else {
          // Positional args fallback
          if (pkg_path.empty()) {
            pkg_path = arg;
          } else if (out_dir == ".") {
            out_dir = arg;
          } else if (!use_rif) {
            rif_path = arg;
            use_rif = true;
          }
        }
      }

      std::cerr << "DEBUG: pkg_path=" << pkg_path << std::endl;
      std::cerr.flush();

      if (pkg_path.empty()) {
        std::cerr << "Error: PKG file must be specified" << std::endl;
        showUsage(argv[0]);
        return 1;
      }

      std::cerr << "DEBUG: About to check RIF" << std::endl;
      std::cerr.flush();

      if (use_rif) {
        std::cout << "Using RIF file: " << rif_path << std::endl;
        if (!RIFGenerator::ValidateRIF(rif_path)) {
          std::cerr << "Warning: The specified RIF file does not appear valid"
                    << std::endl;
        }
      }

      std::cerr << "DEBUG: About to open PKG" << std::endl;
      std::cerr << "DEBUG: export_project=" << export_project
                << " export_dir=" << export_dir.string() << std::endl;
      std::cerr.flush();

      // Continue with PKG extraction
      std::cerr << "DEBUG: PKG File: " << pkg_path.string() << std::endl;
      std::cerr << "DEBUG: Output Folder: " << out_dir.string() << std::endl;
      std::cerr.flush();

      std::cerr << "DEBUG: Checking if file exists..." << std::endl;
      std::cerr.flush();

      // Verify that the file exists
      if (!std::filesystem::exists(pkg_path)) {
        std::cerr << "Error: File does not exist: " << pkg_path.string()
                  << std::endl;
        return 1;
      }

      std::cerr << "DEBUG: File exists, detecting type..." << std::endl;
      std::cerr.flush();

      // Detect file type
      FileType fileType = detectFileType(pkg_path);
      std::cout << "Detected file type: ";

      std::filesystem::path exePath;

      if (fileType == FileType::ELF) {
        std::cout << "ELF Binary\n";

        // For ELF files, use the file directly as the executable
        exePath = pkg_path;

        // Create output directory if needed for asset export
        if (!std::filesystem::exists(out_dir)) {
          std::filesystem::create_directories(out_dir);
          LOG_INFO(Common, "Created output folder: {}", out_dir.string());
        }
      } else if (fileType == FileType::PKG) {
        std::cout << "PS4 PKG\n";

        // Create the output folder if it doesn't exist
        if (!std::filesystem::exists(out_dir)) {
          std::filesystem::create_directories(out_dir);
          LOG_INFO(Common, "Created output folder: {}", out_dir.string());
        }

        std::string failreason;
        PKG pkg;
        if (!pkg.Open(pkg_path, failreason)) {
          std::cerr << "Error opening PKG file: " << failreason << std::endl;
          return 1;
        }

        LOG_INFO(Common, "PKG file opened successfully!");

        // If a RIF file was provided, try to use it for decryption
        if (use_rif) {
          LOG_INFO(Common, "Attempting decryption with RIF file...");
          // TODO: Implement RIF file integration with PKG decryption
        }

        // Extraction and decryption
        // Use Scan instead of Extract, as Scan is verified to work correctly
        // for parsing
        if (!pkg.Scan(pkg_path, failreason, out_dir)) {
          LOG_ERROR(Lib_Kernel, "Error during scanning/decryption: {}",
                    failreason);
          return 1;
        }

        // Extract the actual files with progress bar
        pkg.ExtractAllFilesWithProgress();

        std::cout << "Extraction and decryption completed successfully!\n";

        // Find executable for PKG
        std::filesystem::path potentialEboot = out_dir / "eboot.bin";
        std::filesystem::path potentialEbootUpper = out_dir / "EBOOT.BIN";

        if (std::filesystem::exists(potentialEboot))
          exePath = potentialEboot;
        else if (std::filesystem::exists(potentialEbootUpper))
          exePath = potentialEbootUpper;
        else {
          // Search for SPRX
          for (const auto &entry :
               std::filesystem::recursive_directory_iterator(out_dir)) {
            if (entry.path().extension() == ".sprx") {
              exePath = entry.path();
              break;
            }
          }
        }
      } else if (fileType == FileType::ISO_PS2 ||
                 fileType == FileType::ISO_PS3) {
        std::cout << (fileType == FileType::ISO_PS3 ? "PS3 ISO" : "PS2 ISO")
                  << "\n";

        // Create the output folder if it doesn't exist
        if (!std::filesystem::exists(out_dir)) {
          std::filesystem::create_directories(out_dir);
          LOG_INFO(Common, "Created output folder: {}", out_dir.string());
        }

        // Extract ISO
        ShadPKG::ISOExtractor isoExtractor;
        auto result = isoExtractor.extract(pkg_path, out_dir);

        if (!result.success) {
          std::cerr << "Error extracting ISO: " << result.errorMessage
                    << std::endl;
          return 1;
        }

        std::cout << "ISO extraction completed successfully!\n";
        if (!result.gameTitle.empty()) {
          std::cout << "Game: " << result.gameTitle << "\n";
        }
        if (!result.gameID.empty()) {
          std::cout << "Game ID: " << result.gameID << "\n";
        }

        exePath = result.extractedExecutable;
      } else {
        std::cerr << "Error: Unknown file type. Must be PKG or ISO.\n";
        return 1;
      }

      // --- Decompiler Integration ---
      if (export_project || list_functions || decompile_single) {
        LOG_INFO(Common, "Starting Decompiler Analysis...");

        // exePath already found above
        if (exePath.empty()) {
          std::cerr << "Error: Could not find executable in " << out_dir
                    << "\n";
          return 1;
        }

        LOG_INFO(Common, "Found executable: {}", exePath.string());

        // 2. Load into DecompilerContext
        auto data = readFile(exePath);
        if (data.empty()) {
          std::cerr << "Error: Failed to read executable file.\n";
          return 1;
        }

        auto &ctx = ShadPKG::Decompiler::DecompilerContext::Get();
        if (!ctx.LoadELF(data)) {
          std::cerr << "Error: Failed to parse ELF file.\n";
          return 1;
        }

        if (!load_db_path.empty()) {
          LOG_INFO(Common, "Loading project database from: {}",
                   load_db_path.string());
          if (ctx.LoadProject(load_db_path.string())) {
            LOG_INFO(Common, "Project database loaded successfully.");
          } else {
            LOG_ERROR(Common, "Failed to load project database.");
            return 1;
          }
        }

        // 3. Analyze
        LOG_INFO(Common, "Analyzing binary (this may take a while)...");
        ctx.Analyze();
        LOG_INFO(Common, "Analysis complete. Found {} functions.",
                 ctx.GetFunctions().size());

        // 4. Handle Commands
        if (list_functions) {
          std::cout << "\n--- Function List ---\n";
          std::cout << "Address      | Name\n";
          std::cout << "-------------|-------------------\n";
          for (const auto &func : ctx.GetFunctions()) {
            printf("0x%010llX | %s\n", func->address, func->name.c_str());
          }
        }

        if (decompile_single) {
          auto func = ctx.GetFunctionAt(decompile_addr);
          if (!func) {
            std::cerr << "Error: Function at 0x" << std::hex << decompile_addr
                      << " not found.\n";
          } else {

            using namespace ShadPKG::Decompiler;

            // Init Global Analysis
            auto symbols = std::make_shared<Analysis::SymbolAnalysis>(
                ctx.GetRawData(), ctx.GetBaseAddress(),
                ctx.GetSymbolDatabase());
            symbols->analyze();

            auto dom = std::make_shared<Analysis::DominatorAnalysis>();
            dom->analyze(func);

            Analysis::StructuralAnalysis structural(func, dom, symbols);
            auto ast = structural.analyze();

            Lifter::VariableAnalysis lifter(func);
            lifter.analyze();
            lifter.applyToAST(ast);

            Analysis::DataFlowAnalysis dataflow(ast);
            dataflow.analyze();

            Analysis::MemberAccessAnalysis memberAccess(ctx.GetTypeManager());
            memberAccess.analyze(ast);

            Codegen::CppEmitter emitter;
            std::cout << emitter.generate(ast) << "\n";
          }
        }

        if (export_project) {
          LOG_INFO(Common, "Exporting project to: {}", export_dir.string());
          ctx.ExportProject(export_dir.string());

          // Copy assets
          try {
            std::filesystem::path assetDest = export_dir / "assets";
            std::filesystem::create_directories(assetDest);

            LOG_INFO(Common, "Copying assets from {} to {}...",
                     out_dir.string(), assetDest.string());

            // Get absolute paths for comparison to avoid recursion
            std::filesystem::path absOut = std::filesystem::absolute(out_dir);
            std::filesystem::path absExport =
                std::filesystem::absolute(export_dir);
            std::filesystem::path absAssetDest =
                std::filesystem::absolute(assetDest);

            for (const auto &entry :
                 std::filesystem::directory_iterator(out_dir)) {
              try {
                std::filesystem::path entryAbs =
                    std::filesystem::absolute(entry.path());

                // Use equivalent() for robust path comparison
                if (std::filesystem::equivalent(entryAbs, absExport)) {
                  LOG_INFO(Common, "Skipping export directory: {}",
                           entryAbs.string());
                  continue;
                }

                if (std::filesystem::equivalent(entryAbs, absAssetDest)) {
                  LOG_INFO(Common, "Skipping asset destination directory: {}",
                           entryAbs.string());
                  continue;
                }

                // Copy entry
                std::filesystem::copy(
                    entry.path(), assetDest / entry.path().filename(),
                    std::filesystem::copy_options::recursive |
                        std::filesystem::copy_options::overwrite_existing);
              } catch (...) {
                // entries that don't exist or other errors
                continue;
              }
            }
          } catch (const std::exception &e) {
            LOG_ERROR(Common, "Failed to copy assets: {}", e.what());
          }

          LOG_INFO(Common, "Export complete.");
        }
      }

      return 0;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    // PATCH COMMAND - PSN Bypass
    // ═══════════════════════════════════════════════════════════════════════════
    if (command == "patch") {
      std::string input_file;
      std::string output_file = "eboot_patched.bin";
      std::string game_id = "CUSA00265"; // Default: Minecraft EU
      std::string game_version = "01.00";

      bool scan_only = false;
      bool gen_options = false;
      bool convert_elf = false;

      // Parse arguments
      for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];
        if ((arg == "-i" || arg == "--input") && i + 1 < argc) {
          input_file = argv[++i];
        } else if ((arg == "-o" || arg == "--output") && i + 1 < argc) {
          output_file = argv[++i];
        } else if (arg == "--game" && i + 1 < argc) {
          game_id = argv[++i];
        } else if (arg == "--version" && i + 1 < argc) {
          game_version = argv[++i];
        } else if (arg == "--scan") {
          scan_only = true;
        } else if (arg == "--gen-options") {
          gen_options = true;
        } else if (arg == "--elf") {
          convert_elf = true;
        } else if (arg[0] != '-' && input_file.empty()) {
          input_file = arg;
        }
      }

      // Generate Minecraft options.txt
      if (gen_options) {
        std::cout << "=== Minecraft PSN Bypass options.txt ===" << std::endl;
        std::cout << ShadPKG::Patcher::PSNBypass::generateMinecraftOptions()
                  << std::endl;
        std::cout << "\nSave this content to your Minecraft save folder:"
                  << std::endl;
        std::cout << "/mnt/pfs/"
                     "savedata_XXXXXXXX_CUSA00265_BedrockUserSettingsStorage/"
                     "options.txt"
                  << std::endl;
        return 0;
      }

      if (input_file.empty()) {
        std::cerr << "Error: No input file specified for patch command."
                  << std::endl;
        std::cerr << "Usage: " << argv[0]
                  << " patch -i <eboot.bin> [-o <output.bin>]" << std::endl;
        return 1;
      }

      ShadPKG::Patcher::PSNBypass patcher;

      if (!patcher.loadEboot(input_file)) {
        std::cerr << "Error: Failed to load eboot file: " << input_file
                  << std::endl;
        return 1;
      }

      if (scan_only) {
        std::cout << "=== Scanning for PSN functions ===" << std::endl;
        patcher.findPSNFunctions();
        return 0;
      }

      // Apply patches
      std::cout << "=== Applying PSN Bypass Patches ===" << std::endl;
      std::cout << "Game ID: " << game_id << std::endl;
      std::cout << "Version: " << game_version << std::endl;

      auto patchSet =
          ShadPKG::Patcher::PSNBypass::getMinecraftPatches(game_version);
      patchSet.gameId = game_id;

      bool applied = patcher.applyPatchSet(patchSet);

      if (applied) {
        std::cout << "[PSNBypass] Patches applied successfully." << std::endl;
      } else {
        std::cout << "[PSNBypass] No patches applied (Conditions not met or "
                     "disabled)."
                  << std::endl;
      }

      // Save output if patches applied OR if we are converting
      if (applied || convert_elf) {
        if (patcher.saveEboot(output_file)) {
          std::cout << "Success! Eboot saved to: " << output_file << std::endl;

          if (convert_elf) {
            std::cout << "Converting to ELF64..." << std::endl;
            ShadPKG::Patcher::ElfReconstructor reconstructor;
            if (reconstructor.loadFile(output_file)) {
              if (reconstructor.reconstructElf(output_file)) { // Overwrite
                std::cout << "Converted patched SELF to ELF: " << output_file
                          << std::endl;
              } else {
                std::cerr << "Failed to convert to ELF." << std::endl;
                return 1;
              }
            } else {
              std::cerr << "Failed to reload patched file for conversion."
                        << std::endl;
              return 1;
            }
          }

          std::cout << "\nNext steps:" << std::endl;
          std::cout << "1. Replace the original eboot.bin with the patched one"
                    << std::endl;
          std::cout << "2. Generate options.txt: " << argv[0]
                    << " patch --gen-options" << std::endl;
          std::cout << "3. Copy options.txt to your Minecraft save folder"
                    << std::endl;
        } else {
          std::cerr << "Error: Failed to save patched eboot" << std::endl;
          return 1;
        }
      } else {
        std::cerr
            << "Warning: No patches were applied and no conversion requested."
            << std::endl;
        return 1;
      }

      return 0;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    // PATCH-PKG COMMAND - Complete PKG Patcher
    // ═══════════════════════════════════════════════════════════════════════════
    if (command == "patch-pkg") {
      std::string input_pkg;
      std::string output_pkg;
      std::string game_id = "CUSA00265";
      bool extract_only = false;

      // Parse arguments
      for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];
        if ((arg == "-i" || arg == "--input") && i + 1 < argc) {
          input_pkg = argv[++i];
        } else if ((arg == "-o" || arg == "--output") && i + 1 < argc) {
          output_pkg = argv[++i];
        } else if (arg == "--game" && i + 1 < argc) {
          game_id = argv[++i];
        } else if (arg == "--extract-only") {
          extract_only = true;
        } else if (arg[0] != '-' && input_pkg.empty()) {
          input_pkg = arg;
        } else if (arg[0] != '-' && !input_pkg.empty() && output_pkg.empty()) {
          output_pkg = arg;
        }
      }

      if (input_pkg.empty()) {
        std::cerr << "Error: No input PKG specified." << std::endl;
        std::cerr << "Usage: " << argv[0]
                  << " patch-pkg -i <input.pkg> [-o <output.pkg>]" << std::endl;
        return 1;
      }

      if (output_pkg.empty()) {
        std::filesystem::path pkg_path(input_pkg);
        output_pkg = (pkg_path.parent_path() /
                      (pkg_path.stem().string() + "_patched.pkg"))
                         .string();
      }

      std::cout << "╔══════════════════════════════════════════════════════════"
                   "══════╗"
                << std::endl;
      std::cout << "║         Complete PKG Patcher - Minecraft PS4 PSN Bypass  "
                   "      ║"
                << std::endl;
      std::cout << "╚══════════════════════════════════════════════════════════"
                   "══════╝"
                << std::endl;
      std::cout << "\nInput PKG:  " << input_pkg << std::endl;
      std::cout << "Output PKG: " << output_pkg << std::endl;
      std::cout << "Game ID:    " << game_id << std::endl;

      // Step 1: Extract PKG using subprocess (more reliable for large files)
      std::cout << "\n[1/3] Extracting PKG..." << std::endl;
      std::filesystem::path extract_dir =
          std::filesystem::temp_directory_path() / "shadpkg_patch";
      std::filesystem::create_directories(extract_dir);

      std::string extract_cmd = std::string(argv[0]) + " extract --input \"" +
                                input_pkg + "\" --export-project \"" +
                                extract_dir.string() + "\"";
      int extract_result = system(extract_cmd.c_str());

      if (extract_result != 0) {
        std::cerr << "Error: Failed to extract PKG" << std::endl;
        return 1;
      }

      std::cout << "[+] PKG extracted to: " << extract_dir << std::endl;

      // Step 2: Find and scan eboot.bin
      std::cout << "\n[2/3] Scanning for PSN functions..." << std::endl;
      std::vector<std::filesystem::path> eboot_files;
      for (const auto &entry :
           std::filesystem::recursive_directory_iterator(extract_dir)) {
        if (entry.path().filename() == "eboot.bin") {
          eboot_files.push_back(entry.path());
        }
      }

      if (eboot_files.empty()) {
        std::cerr << "Error: eboot.bin not found in extracted PKG" << std::endl;
        return 1;
      }

      std::filesystem::path eboot_bin = eboot_files[0];
      std::cout << "[+] Found eboot.bin: " << eboot_bin << std::endl;

      ShadPKG::Patcher::PSNBypass patcher;
      if (!patcher.loadEboot(eboot_bin.string())) {
        std::cerr << "Error: Failed to load eboot.bin" << std::endl;
        return 1;
      }

      auto psnOffsets = patcher.findPSNFunctions();
      std::cout << "[+] Found " << psnOffsets.size() << " PSN functions/strings"
                << std::endl;

      // Apply patches to eboot.bin
      std::cout << "[*] Applying PSN bypass patches..." << std::endl;
      int patches_applied = 0;

      // Search for the specific PSN_Auth_Check pattern: 85 C0 74 (test eax,
      // eax; jz) This is the actual authentication check that needs to be
      // bypassed
      std::vector<uint8_t> psn_check_pattern = {0x85, 0xC0, 0x74};
      auto auth_check_offsets = patcher.searchPattern(psn_check_pattern);

      std::cout << "[*] Found " << auth_check_offsets.size()
                << " PSN_Auth_Check patterns (85 C0 74)" << std::endl;

      // Patch each PSN_Auth_Check pattern
      // Replace "test eax, eax; jz" with "xor eax, eax; ret" to always return
      // success
      std::vector<uint8_t> return_success = {0x31, 0xC0, 0xC3};

      for (const auto &offset : auth_check_offsets) {
        // Only patch if it looks like code (not in string section)
        // Verify by checking surrounding bytes
        if (offset > 0x1000 &&
            offset < 0x800000) { // Reasonable code section bounds
          if (patcher.patchOffset(offset, return_success)) {
            patches_applied++;
            std::cout << "[+] Patched PSN_Auth_Check at 0x" << std::hex
                      << offset << std::dec << std::endl;
          }
        }
      }

      std::cout << "[+] Applied " << patches_applied
                << " PSN_Auth_Check patches" << std::endl;

      // Save patched eboot.bin
      if (!patcher.saveEboot(eboot_bin.string())) {
        std::cerr << "Warning: Could not save patched eboot.bin, continuing..."
                  << std::endl;
      } else {
        std::cout << "[+] Patched eboot.bin saved" << std::endl;
      }

      // Step 3: Generate options.txt
      std::cout << "\n[3/3] Generating PSN bypass configuration..."
                << std::endl;
      std::filesystem::path options_file = extract_dir / "options.txt";
      std::string options_content =
          ShadPKG::Patcher::PSNBypass::generateMinecraftOptions();

      std::ofstream options_out(options_file);
      options_out << options_content;
      options_out.close();
      std::cout << "[+] Generated: " << options_file << std::endl;

      if (extract_only) {
        std::cout << "\n[+] Extract-only mode. Files ready at: " << extract_dir
                  << std::endl;
        std::cout << "To build the final PKG, use Patch Builder or:"
                  << std::endl;
        std::cout
            << "  orbis-pub-cmd image_build --image_type pkg --image_path \""
            << extract_dir << "\" --output_path \"" << output_pkg << "\""
            << std::endl;
        return 0;
      }

      // Try to build final PKG
      std::cout << "\n[*] Attempting to build final PKG..." << std::endl;
      std::string build_cmd =
          "orbis-pub-cmd image_build --image_type pkg --image_path \"" +
          extract_dir.string() + "\" --output_path \"" + output_pkg + "\"";

      int result = system(build_cmd.c_str());

      if (result == 0 && std::filesystem::exists(output_pkg)) {
        auto size = std::filesystem::file_size(output_pkg);
        std::cout << "\n╔══════════════════════════════════════════════════════"
                     "══════════╗"
                  << std::endl;
        std::cout << "║                    SUCCESS!                            "
                     "        ║"
                  << std::endl;
        std::cout << "╚════════════════════════════════════════════════════════"
                     "════════╝"
                  << std::endl;
        std::cout << "\nPatched PKG: " << output_pkg << std::endl;
        std::cout << "Size: " << (size / (1024 * 1024)) << " MB" << std::endl;
        std::cout << "\nReady to install on PS4!" << std::endl;
        std::cout << "\nNEXT STEPS:" << std::endl;
        std::cout << "1. Copy PKG to PS4 via USB or FTP" << std::endl;
        std::cout << "2. Use Package Manager to install" << std::endl;
        std::cout << "3. Start Minecraft and configure multiplayer"
                  << std::endl;
        std::cout << "4. Copy options.txt to save data for PSN bypass"
                  << std::endl;
        return 0;
      } else {
        std::cout << "\n[!] orbis-pub-cmd not available or failed" << std::endl;
        std::cout << "Extracted files are ready at: " << extract_dir
                  << std::endl;
        std::cout << "\nTo build the PKG, install LibOrbisPkg and run:"
                  << std::endl;
        std::cout
            << "  orbis-pub-cmd image_build --image_type pkg --image_path \""
            << extract_dir << "\" --output_path \"" << output_pkg << "\""
            << std::endl;
        std::cout << "\nOr use Patch Builder GUI:" << std::endl;
        std::cout << "  "
                     "https://www.mediafire.com/file/xw0zn2e0rjaf5k7/"
                     "Patch_Builder_v1.3.3.zip/file"
                  << std::endl;
        return 0;
      }
    }

    // Unrecognized command
    std::cerr << "Unrecognized command: " << command << std::endl;
    showUsage(argv[0]);
    return 1;
  } catch (const std::exception &e) {
    std::cerr << "Unhandled C++ exception: " << e.what() << std::endl;
    return 2;
  } catch (...) {
    std::cerr << "Fatal error: crash or unhandled exception." << std::endl;
    return 3;
  }
}