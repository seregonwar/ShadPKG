// SPDX-FileCopyrightText: Copyright 2025 shadPKG 
// SPDX-License-Identifier: GPL-2.0-or-later

#include <iostream>
#include <filesystem>
#include <string>
#include "core/file_format/pkg.h"
#include "core/file_format/rif_generator.h"
#include "common/logging/backend.h"
#include "common/logging/log.h"
#include "simple_log.h"

void showUsage(const char* program_name) {
    std::cout << R"(
Estrattore/Decifratore PKG PS4 con Generatore RIF - by seregonwar
https://github.com/seregonwar
---------------------------------------------

Uso:
  )" << program_name << R"( extract [opzioni] <file.pkg> [cartella_output] [file.rif]
  )" << program_name << R"( generate-rif <content_id> [output_dir]
  )" << program_name << R"( validate-rif <file.rif>
  )" << program_name << R"( pfs-info [opzioni] <file.pkg>

Comandi:
  extract       - Estrae e decifra un file PKG
                  Se specificato, usa il file RIF per la decifrazione
  generate-rif  - Genera un file RIF per il Content ID specificato
  validate-rif  - Valida un file RIF esistente
  pfs-info      - Scansiona il PKG e mostra la struttura PFS senza estrarre file

Opzioni generali:
  -h, --help    - Mostra questo help

Opzioni per extract:
  -o, --output <dir>  - Directory di output (fallback: crea sottocartella con TitleID)
  -r, --rif <file>    - Percorso del file RIF da usare durante la decifratura

Opzioni per pfs-info:
  --json        - Stampa l'output in formato JSON su stdout (per integrazione subprocess)

Esempi:
  )" << program_name << R"( extract game.pkg ./output
  )" << program_name << R"( extract game.pkg ./output game.rif
  )" << program_name << R"( extract -o ./output -r game.rif game.pkg
  )" << program_name << R"( generate-rif EP0001-CUSA12345_00-TESTGAMERETAIL01
  )" << program_name << R"( validate-rif EP0001-CUSA12345_00-TESTGAMERETAIL01.rif
  )" << program_name << R"( pfs-info game.pkg
  )" << program_name << R"( pfs-info --json game.pkg
)";
}

int main(int argc, char* argv[]) {
    // Inizializza il logger globale (stampa su console e file)
    Common::Log::Initialize("estrazione_pkg.log");
    Common::Log::SetColorConsoleBackendEnabled(true);
    simple_log("[START] Avvio estrattore PKG con generatore RIF");

    LOG_INFO(Common, "Test log: il logger funziona!");

    try {
        if (argc < 2) {
            showUsage(argv[0]);
            return 1;
        }

        std::string command = argv[1];

        // Help globale
        if (command == "-h" || command == "--help" || command == "-help" || command == "-?") {
            showUsage(argv[0]);
            return 0;
        }

        // Comando per generare file RIF
        if (command == "generate-rif") {
            if (argc < 3) {
                std::cerr << "Errore: Content ID richiesto per generate-rif\n";
                showUsage(argv[0]);
                return 1;
            }

            std::string content_id = argv[2];
            std::filesystem::path output_dir = (argc >= 4) ? argv[3] : ".";

            RIFGenerator generator;
            if (generator.GenerateRIF(content_id, output_dir)) {
                std::cout << "File RIF generato con successo per: " << content_id << std::endl;
                return 0;
            } else {
                std::cerr << "Errore nella generazione del file RIF" << std::endl;
                return 1;
            }
        }

        // Comando per validare file RIF
        if (command == "validate-rif") {
            if (argc < 3) {
                std::cerr << "Errore: File RIF richiesto per validate-rif\n";
                showUsage(argv[0]);
                return 1;
            }

            std::filesystem::path rif_path = argv[2];
            if (RIFGenerator::ValidateRIF(rif_path)) {
                std::cout << "File RIF valido: " << rif_path << std::endl;
                return 0;
            } else {
                std::cerr << "File RIF non valido: " << rif_path << std::endl;
                return 1;
            }
        }

        // Comando leggero: pfs-info
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
                    std::cerr << "Opzione non riconosciuta: " << arg << std::endl;
                    showUsage(argv[0]);
                    return 1;
                } else if (pkg_path.empty()) {
                    pkg_path = arg;
                } else {
                    std::cerr << "Argomenti in eccesso per pfs-info" << std::endl;
                    return 1;
                }
            }

            if (pkg_path.empty()) {
                std::cerr << "Errore: specificare il file PKG" << std::endl;
                showUsage(argv[0]);
                return 1;
            }

            std::string failreason;
            PKG pkg;
            if (!pkg.Scan(pkg_path, failreason)) {
                std::cerr << "Errore in pfs-info: " << failreason << std::endl;
                return 1;
            }

            // Header/summary
            auto header = pkg.GetPkgHeader();
            auto title_id = pkg.GetTitleID();
            auto flags = pkg.GetPkgFlags();
            auto entries = pkg.GetEntriesInfo();
            size_t n_files = 0, n_dirs = 0;
            for (const auto& e : entries) {
                if (e.type == PFS_FILE) n_files++;
                else if (e.type == PFS_DIR) n_dirs++;
            }

            if (as_json) {
                // Stampa JSON semplice
                std::cout << "{\n";
                std::cout << "  \"title_id\": \"" << std::string(title_id) << "\",\n";
                std::cout << "  \"flags\": \"" << flags << "\",\n";
                std::cout << "  \"pkg_size\": " << pkg.GetPkgSize() << ",\n";
                std::cout << "  \"entries\": [\n";
                for (size_t i = 0; i < entries.size(); ++i) {
                    const auto& e = entries[i];
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
                std::cout << "  \"summary\": { \"files\": " << n_files << ", \"dirs\": " << n_dirs << " }\n";
                std::cout << "}\n";
            } else {
                // Stampa leggibile
                std::cout << "\n--- PFS Info ---\n";
                std::cout << "TitleID: " << title_id << "\n";
                std::cout << "Flags: " << flags << "\n";
                std::cout << "PKG Size: " << pkg.GetPkgSize() << "\n";
                std::cout << "Dirs: " << n_dirs << ", Files: " << n_files << "\n\n";
                for (const auto& e : entries) {
                    std::cout << (e.type == PFS_DIR ? "[D] " : (e.type == PFS_FILE ? "[F] " : "[?] "))
                              << e.path << (e.path.empty() ? e.name : std::string())
                              << (e.type == PFS_FILE ? (" (size=" + std::to_string(e.size) + ")") : "")
                              << "\n";
                }
            }
            return 0;
        }

        // Comando per estrarre PKG
        if (command == "extract") {
            // Parsing flessibile: supporta sia posizionale (storico) che con opzioni -o/--output e -r/--rif
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
                        std::cerr << "Errore: manca il valore per --output" << std::endl;
                        return 1;
                    }
                    out_dir = argv[++i];
                } else if (arg == "-r" || arg == "--rif") {
                    if (i + 1 >= argc) {
                        std::cerr << "Errore: manca il valore per --rif" << std::endl;
                        return 1;
                    }
                    rif_path = argv[++i];
                    use_rif = true;
                } else if (!arg.empty() && arg[0] == '-') {
                    std::cerr << "Opzione non riconosciuta: " << arg << std::endl;
                    showUsage(argv[0]);
                    return 1;
                } else {
                    if (pkg_path.empty()) {
                        pkg_path = arg;
                    } else if (out_dir == ".") {
                        // compat: cartella_output posizionale
                        out_dir = arg;
                    } else if (!use_rif) {
                        // compat: file.rif posizionale
                        rif_path = arg;
                        use_rif = true;
                    } else {
                        std::cerr << "Argomenti in eccesso per extract" << std::endl;
                        return 1;
                    }
                }
            }

            if (pkg_path.empty()) {
                std::cerr << "Errore: specificare il file PKG" << std::endl;
                showUsage(argv[0]);
                return 1;
            }

            if (use_rif) {
                std::cout << "Usando file RIF: " << rif_path << std::endl;
                if (!RIFGenerator::ValidateRIF(rif_path)) {
                    std::cerr << "Attenzione: Il file RIF specificato non sembra valido" << std::endl;
                }
            }

            // Continua con l'estrazione PKG
            simple_log("[INFO] File PKG: " + pkg_path.string());
            simple_log("[INFO] Cartella output: " + out_dir.string());
            if (use_rif) {
                simple_log("[INFO] File RIF: " + rif_path.string());
            }

            // Verifica che il file PKG esista
            if (!std::filesystem::exists(pkg_path)) {
                LOG_ERROR(Lib_Kernel, "Il file PKG non esiste: {}", pkg_path.string());
                return 1;
            }

            // Verifica che il file RIF esista se specificato
            if (use_rif && !std::filesystem::exists(rif_path)) {
                LOG_ERROR(Lib_Kernel, "Il file RIF non esiste: {}", rif_path.string());
                return 1;
            }

            // Crea la cartella di output se non esiste
            if (!std::filesystem::exists(out_dir)) {
                std::filesystem::create_directories(out_dir);
                simple_log("[INFO] Creata cartella output: " + out_dir.string());
            }

            std::string failreason;
            PKG pkg;
            if (!pkg.Open(pkg_path, failreason)) {
                std::cerr << "Errore nell'apertura del file PKG: " << failreason << std::endl;
                return 1;
            }

            simple_log("[SUCCESS] File PKG aperto con successo!");

            // Se è stato fornito un file RIF, prova a usarlo per la decifrazione
            if (use_rif) {
                simple_log("[RIF] Tentativo di decifrazione con file RIF...");
                // TODO: Implementare l'integrazione del file RIF con la decifrazione PKG
            }

            // Stampa le informazioni del PKG
            auto header = pkg.GetPkgHeader();
            std::cout << "\n--- Info PKG ---\n";
            std::cout << "TitleID: " << pkg.GetTitleID() << std::endl;
            std::cout << "Flags: " << pkg.GetPkgFlags() << std::endl;
            std::cout << "PKG Size: " << pkg.GetPkgSize() << std::endl;

            // Estrazione e decifrazione: prima prepara le strutture, poi estrai i file con progress
            if (!pkg.Extract(pkg_path, out_dir, failreason)) {
                LOG_ERROR(Lib_Kernel, "Errore durante l'estrazione/decifratura: {}", failreason);
                return 1;
            }

            // Estrai i file veri e propri con barra di avanzamento
            pkg.ExtractAllFilesWithProgress();

            std::cout << "Estrazione e decifratura completate con successo!\n";
            return 0;
        }

        // Comando non riconosciuto
        std::cerr << "Comando non riconosciuto: " << command << std::endl;
        showUsage(argv[0]);
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "Eccezione C++ non gestita: " << e.what() << std::endl;
        return 2;
    } catch (...) {
        std::cerr << "Errore fatale: crash o eccezione non gestita." << std::endl;
        return 3;
    }
}
