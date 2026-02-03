import os
import shutil
import subprocess
import sys

# Configure paths
project_root = os.path.abspath(os.path.dirname(__file__))
build_dir = os.path.join(project_root, "build")

def run(cmd, cwd=project_root):
    print(f"\nRunning: {' '.join(cmd)}\n")
    result = subprocess.run(cmd, cwd=cwd)
    if result.returncode != 0:
        print("Error running command:", ' '.join(cmd))
        sys.exit(result.returncode)

def check_conan():
    try:
        subprocess.run(["conan", "--version"], check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    except (FileNotFoundError, subprocess.CalledProcessError):
        print("Conan not found. Please ensure it is installed and in your PATH.")
        print("You can install it with: pip install conan")
        sys.exit(1)

def find_toolchain(search_path):
    for root, dirs, files in os.walk(search_path):
        if "conan_toolchain.cmake" in files:
            return os.path.join(root, "conan_toolchain.cmake")
    return None

if __name__ == "__main__":

    os.makedirs(build_dir, exist_ok=True)

    check_conan()

    # Install dependencies with Conan
    # This command is safe to rerun (idempotent if nothing changes)
    conan_cmd = [
        "conan", "install", ".",
        "--output-folder=build",
        "--build=missing",
        "-s", "build_type=Release"
    ]
    run(conan_cmd)

    # Find the toolchain file
    toolchain_path = find_toolchain(build_dir)
    if not toolchain_path:
        print("Error: conan_toolchain.cmake not found after installation.")
        sys.exit(1)
    
    print(f"Toolchain found: {toolchain_path}")
    
    # Relative path for cmake (or absolute, but cmake expects paths with correct / or \)
    # Use absolute path for safety
    toolchain_path = toolchain_path.replace("\\", "/")

    # CMake Configuration
    cmake_cmd = [
        "cmake",
        "-S", ".",
        "-B", "build",
        f"-DCMAKE_TOOLCHAIN_FILE={toolchain_path}",
        "-DCMAKE_BUILD_TYPE=Release",
        "-DBUILD_GUI=ON"
    ]
    
    # Add -DCMAKE_POLICY_DEFAULT_CMP0091=NEW to correctly handle MSVC runtime with Conan
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

    print("\nBuild completed successfully!")
