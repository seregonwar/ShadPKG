// SPDX-FileCopyrightText: Copyright 2025 shadPKG
// SPDX-License-Identifier: GPL-2.0-or-later

#include "common/logging/backend.h"
#include "common/logging/log.h"
#include "core/decompiler/DecompilerContext.h"
#include "core/decompiler/analysis/MemberAccessAnalysis.h"
#include "core/file_format/pkg.h"
#include "core/file_format/psf.h"
#include "core/file_format/rif_generator.h"
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

Commands:
  extract       - Extracts and decrypts a PKG file and optionally decompiles it.
  generate-rif  - Generates a RIF file for the specified Content ID
  validate-rif  - Validates an existing RIF file
  pfs-info      - Scans the PKG and shows the PFS structure without extracting files
  sfo-info      - Displays param.sfo parameters from a PKG file

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
             std::cerr << "Error: missing value for --export-project" << std::endl;
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

      // Smart Output Path adjustment if not explicitly set by user to something else than . or arg
      // But we respect out_dir from above.
      
      // Extraction and decryption
      if (!pkg.Extract(pkg_path, out_dir, failreason)) {
        LOG_ERROR(Lib_Kernel, "Error during extraction/decryption: {}",
                  failreason);
        return 1;
      }

      // Extract the actual files with progress bar
      pkg.ExtractAllFilesWithProgress();

      std::cout << "Extraction and decryption completed successfully!\n";

      // --- Decompiler Integration ---
      if (export_project || list_functions || decompile_single) {
         LOG_INFO(Common, "Starting Decompiler Analysis...");

         // 1. Locate Executable (eboot.bin or .sprx)
         std::filesystem::path exePath;
         std::filesystem::path potentialEboot = out_dir / "eboot.bin";
         std::filesystem::path potentialEbootUpper = out_dir / "EBOOT.BIN";
         
         if (std::filesystem::exists(potentialEboot)) exePath = potentialEboot;
         else if (std::filesystem::exists(potentialEbootUpper)) exePath = potentialEbootUpper;
         else {
            // Search for SPRX
            for (const auto& entry : std::filesystem::recursive_directory_iterator(out_dir)) {
               if (entry.path().extension() == ".sprx") {
                  exePath = entry.path();
                  break;
               }
            }
         }

         if (exePath.empty()) {
            std::cerr << "Error: Could not find eboot.bin or .sprx in " << out_dir << "\n";
            return 1;
         }

         LOG_INFO(Common, "Found executable: {}", exePath.string());

         // 2. Load into DecompilerContext
         auto data = readFile(exePath);
         if (data.empty()) {
             std::cerr << "Error: Failed to read executable file.\n";
             return 1;
         }
         
         auto& ctx = ShadPKG::Decompiler::DecompilerContext::Get();
         if (!ctx.LoadELF(data)) {
             std::cerr << "Error: Failed to parse ELF file.\n";
             return 1;
         }

         if (!load_db_path.empty()) {
             LOG_INFO(Common, "Loading project database from: {}", load_db_path.string());
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
         LOG_INFO(Common, "Analysis complete. Found {} functions.", ctx.GetFunctions().size());

         // 4. Handle Commands
         if (list_functions) {
             std::cout << "\n--- Function List ---\n";
             std::cout << "Address      | Name\n";
             std::cout << "-------------|-------------------\n";
             for (const auto& func : ctx.GetFunctions()) {
                 printf("0x%010llX | %s\n", func->address, func->name.c_str());
             }
         }

         if (decompile_single) {
             auto func = ctx.GetFunctionAt(decompile_addr);
             if (!func) {
                 std::cerr << "Error: Function at 0x" << std::hex << decompile_addr << " not found.\n";
             } else {
                 // For single function, we need to run the pipeline manually or use a helper
                 // Reuse ExportProject logic? No, that's too heavy.
                 // We need a helper to generate code for ONE function.
                 // But DecompilerContext::GenerateStructuredCode() runs on ALL functions.
                 // I'll create a quick localized pipeline here for simplicity as I can't modify the header easily again without context switch.
                 // Actually, DecompilerContext::ExportProject uses the pipeline on a loop. I can copy that.
                 
                 using namespace ShadPKG::Decompiler;
                 
                 // Init Global Analysis
                 auto symbols = std::make_shared<Analysis::SymbolAnalysis>(ctx.GetRawData(), ctx.GetBaseAddress(), ctx.GetSymbolDatabase());
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
             LOG_INFO(Common, "Export complete.");
         }
      }

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