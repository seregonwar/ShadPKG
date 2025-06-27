// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <iostream>
#include <filesystem>
#include <string>
#include "core/file_format/pkg.h"
#include "common/logging/backend.h"
#include "common/logging/log.h"
#include "simple_log.h"

int main(int argc, char* argv[]) {
    // Inizializza il logger globale (stampa su console e file)
    Common::Log::Initialize("estrazione_pkg.log");
    Common::Log::SetColorConsoleBackendEnabled(true);
    simple_log("[START] Avvio estrattore PKG");

    LOG_INFO(Common, "Test log: il logger funziona!");

    try {
        std::cout << R"(
                                         
Estrattore/Decifratore PKG PS4 - by seregonwar
https://github.com/seregonwar
---------------------------------------------
)";

        if (argc < 3) {
            LOG_ERROR(Lib_Kernel, "Uso: {} <file.pkg> <cartella_output>", argv[0]);
            return 1;
        }

        std::filesystem::path pkg_path = argv[1];
        std::filesystem::path out_dir = argv[2];
        std::string failreason;

        PKG pkg;
        if (!pkg.Open(pkg_path, failreason)) {
            std::cerr << "Errore nell'apertura del file PKG: " << failreason << std::endl;
            return 1;
        }

        // Stampa le chiavi derivate solo se Open è andato a buon fine
        auto header = pkg.GetPkgHeader();
        std::cout << "\n--- Info PKG ---\n";
        std::cout << "TitleID: " << pkg.GetTitleID() << std::endl;
        std::cout << "Flags: " << pkg.GetPkgFlags() << std::endl;
        std::cout << "PKG Size: " << pkg.GetPkgSize() << std::endl;

        // Le chiavi derivate sono membri privati, quindi serve aggiungere metodi pubblici per accedervi
        // Aggiungo metodi getter per dk3_, ivKey, imgKey, ekpfsKey, dataKey, tweakKey
        // (questi metodi vanno aggiunti in pkg.h/cpp)
        auto print_hex = [](const std::string& name, const auto& arr) {
            std::cout << name << ": ";
            for (auto b : arr) std::cout << std::hex << (int)b << " ";
            std::cout << std::dec << std::endl;
        };
        print_hex("DK3", pkg.GetDK3());
        print_hex("IVKey", pkg.GetIVKey());
        print_hex("ImgKey", pkg.GetImgKey());
        print_hex("EkpfsKey", pkg.GetEkpfsKey());
        print_hex("DataKey", pkg.GetDataKey());
        print_hex("TweakKey", pkg.GetTweakKey());

        // Stampa la lista dettagliata delle entry trovate nel PKG
        const char* type_str[] = {"?", "?", "FILE", "DIR", "CURDIR", "PARENTDIR"};
        auto entries = pkg.GetAllEntries();
        std::cout << "\n--- Entry nel PKG (nome | tipo | inode) ---\n";
        for (const auto& [name, inode, type] : entries) {
            std::cout << name << " | " << (type < 6 ? type_str[type] : std::to_string(type)) << " | " << inode << std::endl;
        }

        // Estrazione e decifrazione
        if (!pkg.Extract(pkg_path, out_dir, failreason)) {
            LOG_ERROR(Lib_Kernel, "Errore durante l'estrazione/decifratura: {}", failreason);
            return 1;
        }
        std::cout << "Numero di file trovati: " << pkg.GetNumberOfFiles() << std::endl;
        std::cout << "Contenuto fsTable:" << std::endl;
        for (u32 i = 0; i < entries.size(); ++i) {
            std::string logmsg = "Entry: nome=" + std::get<0>(entries[i]) + " | tipo=" + std::to_string(std::get<2>(entries[i])) + " | inode=" + std::to_string(std::get<1>(entries[i]));
            simple_log(logmsg);
            std::cout << "  " << std::get<0>(entries[i]) << " | tipo: " << std::get<2>(entries[i]) << " | inode: " << std::get<1>(entries[i]) << std::endl;
        }

        // Estrai tutti i file reali dal PKG
        for (u32 i = 0; i < pkg.GetNumberOfFiles(); ++i) {
            pkg.ExtractFiles(i);
        }
        std::cout << "Estrazione e decifratura completate con successo!\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Eccezione C++ non gestita: " << e.what() << std::endl;
        return 2;
    } catch (...) {
        std::cerr << "Errore fatale: crash o eccezione non gestita." << std::endl;
        return 3;
    }
}
