import os
import shutil
import subprocess
import sys

# Configura i path
project_root = os.path.abspath(os.path.dirname(__file__))
build_dir = os.path.join(project_root, "build")

def run(cmd, cwd=project_root):
    print(f"\nEseguo: {' '.join(cmd)}\n")
    result = subprocess.run(cmd, cwd=cwd)
    if result.returncode != 0:
        print("Errore durante il comando:", ' '.join(cmd))
        sys.exit(result.returncode)

def check_conan():
    try:
        subprocess.run(["conan", "--version"], check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    except (FileNotFoundError, subprocess.CalledProcessError):
        print("Conan non trovato. Assicurati che sia installato e nel PATH.")
        print("Puoi installarlo con: pip install conan")
        sys.exit(1)

def find_toolchain(search_path):
    for root, dirs, files in os.walk(search_path):
        if "conan_toolchain.cmake" in files:
            return os.path.join(root, "conan_toolchain.cmake")
    return None

if __name__ == "__main__":

    os.makedirs(build_dir, exist_ok=True)

    check_conan()

    # Installazione dipendenze con Conan
    # Questo comando è sicuro da rieseguire (idempotente se non cambia nulla)
    conan_cmd = [
        "conan", "install", ".",
        "--output-folder=build",
        "--build=missing",
        "-s", "build_type=Release"
    ]
    run(conan_cmd)

    # Trova il toolchain file
    toolchain_path = find_toolchain(build_dir)
    if not toolchain_path:
        print("Errore: conan_toolchain.cmake non trovato dopo l'installazione.")
        sys.exit(1)
    
    print(f"Toolchain trovato: {toolchain_path}")
    
    # Path relativo per cmake (o assoluto, ma cmake vuole path con / o \\ corretti)
    # Usa path assoluto per sicurezza
    toolchain_path = toolchain_path.replace("\\", "/")

    # Configurazione CMake
    cmake_cmd = [
        "cmake",
        "-S", ".",
        "-B", "build",
        f"-DCMAKE_TOOLCHAIN_FILE={toolchain_path}",
        "-DCMAKE_BUILD_TYPE=Release"
    ]
    
    # Aggiungi -DCMAKE_POLICY_DEFAULT_CMP0091=NEW per gestire correttamente il runtime MSVC con Conan
    if sys.platform == "win32":
        cmake_cmd.append("-DCMAKE_POLICY_DEFAULT_CMP0091=NEW")

    run(cmake_cmd)

    # Build
    build_cmd = [
        "cmake",
        "--build", "build",
        "--config", "Release"
    ]
    run(build_cmd)

    print("\nCompilazione completata con successo!")
