import os
import shutil
import subprocess
import sys

# Configura i path
project_root = os.path.abspath(os.path.dirname(__file__))
build_dir = os.path.join(project_root, "build")
vcpkg_toolchain = os.path.join(project_root, "vcpkg", "scripts", "buildsystems", "vcpkg.cmake")
vcpkg_prefix = os.path.join(project_root, "vcpkg", "installed", "x64-windows", "share")

# Pulisci la build
if os.path.exists(build_dir):
    print("Pulizia cartella build...")
    shutil.rmtree(build_dir)

os.makedirs(build_dir, exist_ok=True)

# Comando di configurazione
cmake_cmd = [
    "cmake",
    "-S", ".",
    "-B", "build",
    f"-DCMAKE_TOOLCHAIN_FILE={vcpkg_toolchain}",
    "-DVCPKG_TARGET_TRIPLET=x64-windows",
    f"-DCMAKE_PREFIX_PATH={vcpkg_prefix}"
]

# Comando di build
build_cmd = [
    "cmake",
    "--build", "build",
    "--config", "Release"
]

def run(cmd):
    print(f"\nEseguo: {' '.join(cmd)}\n")
    result = subprocess.run(cmd, cwd=project_root)
    if result.returncode != 0:
        print("Errore durante il comando:", ' '.join(cmd))
        sys.exit(result.returncode)

if __name__ == "__main__":
    run(cmake_cmd)
    run(build_cmd)
    print("\nCompilazione completata!") 