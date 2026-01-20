// SPDX-FileCopyrightText: Copyright 2025 shadPKG
// SPDX-License-Identifier: GPL-2.0-or-later

#include "common/logging/backend.h"
#include "common/logging/log.h"
#include "core/file_format/pkg.h"
#include "core/file_format/psf.h"
#include "core/file_format/rif_generator.h"
#include <filesystem>
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

Commands:
  extract       - Extracts and decrypts a PKG file
                  If specified, uses the RIF file for decryption
  generate-rif  - Generates a RIF file for the specified Content ID
  validate-rif  - Validates an existing RIF file
  pfs-info      - Scans the PKG and shows the PFS structure without extracting files
  sfo-info      - Displays param.sfo parameters from a PKG file

General Options:
  -h, --help    - Shows this help

Options for extract:
  -o, --output <dir>  - Output directory (fallback: creates subdirectory with TitleID)
  -r, --rif <file>    - Path to the RIF file to use during decryption

Options for pfs-info/sfo-info:
  --json            - Prints output in JSON format
  -v, --verbose     - Shows additional details (types, sizes)
  -q, --query KEY   - Query a single parameter value

Examples:
  )" << program_name
            << R"( extract game.pkg ./output
  )" << program_name
            << R"( extract game.pkg ./output game.rif
  )" << program_name
            << R"( extract -o ./output -r game.rif game.pkg
  )" << program_name
            << R"( generate-rif EP0001-CUSA12345_00-TESTGAMERETAIL01
  )" << program_name
            << R"( validate-rif EP0001-CUSA12345_00-TESTGAMERETAIL01.rif
  )" << program_name
            << R"( pfs-info game.pkg
  )" << program_name
            << R"( sfo-info game.pkg
  )" << program_name
            << R"( sfo-info -q TITLE_ID game.pkg
  )" << program_name
            << R"( sfo-info --json game.pkg
)";
}

int main(int argc, char *argv[]) {
  // Initialize global logger (prints to console and file)
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
      // Flexible parsing: supports both positional (legacy) and options
      // -o/--output and -r/--rif
      std::filesystem::path pkg_path;
      std::filesystem::path out_dir = "."; // default
      std::filesystem::path rif_path;
      bool use_rif = false;

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
        } else if (arg == "-r" || arg == "--rif") {
          if (i + 1 >= argc) {
            std::cerr << "Error: missing value for --rif" << std::endl;
            return 1;
          }
          rif_path = argv[++i];
          use_rif = true;
        } else if (!arg.empty() && arg[0] == '-') {
          std::cerr << "Unrecognized option: " << arg << std::endl;
          showUsage(argv[0]);
          return 1;
        } else {
          if (pkg_path.empty()) {
            pkg_path = arg;
          } else if (out_dir == ".") {
            // compat: positional output directory
            out_dir = arg;
          } else if (!use_rif) {
            // compat: positional rif file
            rif_path = arg;
            use_rif = true;
          } else {
            std::cerr << "Excess arguments for extract" << std::endl;
            return 1;
          }
        }
      }

      if (pkg_path.empty()) {
        std::cerr << "Error: PKG file must be specified" << std::endl;
        showUsage(argv[0]);
        return 1;
      }

      if (use_rif) {
        std::cout << "Using RIF file: " << rif_path << std::endl;
        if (!RIFGenerator::ValidateRIF(rif_path)) {
          std::cerr << "Warning: The specified RIF file does not appear valid"
                    << std::endl;
        }
      }

      // Continue with PKG extraction
      LOG_INFO(Common, "PKG File: {}", pkg_path.string());
      LOG_INFO(Common, "Output Folder: {}", out_dir.string());
      if (use_rif) {
        LOG_INFO(Common, "RIF File: {}", rif_path.string());
      }

      // Verify that the PKG file exists
      if (!std::filesystem::exists(pkg_path)) {
        LOG_ERROR(Lib_Kernel, "PKG file does not exist: {}", pkg_path.string());
        return 1;
      }

      // Verify that the RIF file exists if specified
      if (use_rif && !std::filesystem::exists(rif_path)) {
        LOG_ERROR(Lib_Kernel, "RIF file does not exist: {}", rif_path.string());
        return 1;
      }

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

      // Smart Output Path: Append CONTENT_ID to output directory
      if (!pkg.sfo.empty()) {
        PSF psf;
        if (psf.Open(pkg.sfo)) {
          if (auto cid = psf.GetString("CONTENT_ID"); cid.has_value()) {
            out_dir /= *cid;
            if (!std::filesystem::exists(out_dir)) {
              std::filesystem::create_directories(out_dir);
              LOG_INFO(Common, "Created specific output folder: {}",
                       out_dir.string());
            }
          }
        }
      }

      // Print PKG information
      auto header = pkg.GetPkgHeader();
      std::cout << "\n--- PKG Info ---\n";
      std::cout << "TitleID: " << pkg.GetTitleID() << std::endl;
      std::cout << "Flags: " << pkg.GetPkgFlags() << std::endl;
      std::cout << "PKG Size: " << pkg.GetPkgSize() << std::endl;

      // Extraction and decryption: first prepare structures, then extract files
      // with progress
      if (!pkg.Extract(pkg_path, out_dir, failreason)) {
        LOG_ERROR(Lib_Kernel, "Error during extraction/decryption: {}",
                  failreason);
        return 1;
      }

      // Extract the actual files with progress bar
      pkg.ExtractAllFilesWithProgress();

      std::cout << "Extraction and decryption completed successfully!\n";
      return 0;
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