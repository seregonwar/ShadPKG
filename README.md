# shadPKG

A tool for deriving PKG packet encryption keys for ps4 written in c++

## Description

**shadPKG** is an advanced tool to extract and decrypt PlayStation 4 PKG files (games, updates, DLC, etc). It allows you to:
- Analyze the internal structure of a PKG
- Extract all contained files and folders
- Decrypt protected data
- Supports game, patch, update, DLC, and homebrew PKGs
- Progress bar and detailed logging

## Requirements
- Windows 10/11 (x64)
- Visual Studio 2022 (Build Tools) or compatible
- Python 3.10+ (for build.py)
- [vcpkg](https://github.com/microsoft/vcpkg) for dependencies (Zlib, CryptoPP, etc)

## Build Instructions

1. **Clone the repository**
2. **Install dependencies with vcpkg**
   - Run `vcpkg/bootstrap-vcpkg.bat`
   - Install required packages (e.g.: `vcpkg install zlib cryptopp`)
3. **Build the project**
   - Run `python build.py` from the project root
   - Binaries will be generated in `build/Release/`

## Usage

From terminal/PowerShell:

```
shadPKG.exe <path_to_file.pkg> <output_folder>
```

**Example:**
```
shadPKG.exe "C:\GAMES\CUSA12345.pkg" C:\extracted\CUSA12345
```

- The program will extract all files and folders into the chosen directory.
- A progress bar and detailed log are shown on the console and saved to `debug_log.txt`.
- Even "unknown" entries (without a name) are extracted as `entry_0x<ID>.bin`.

## Main Features
- Parallel extraction (multi-threaded)
- Automatic key decryption
- Support for standard, update, DLC, and homebrew PKGs
- Detailed logging and persistent log file
- Robust error and path handling

## Notes
- Some special PKGs (patches, updates) may not contain all expected files.
- In case of issues, check the `debug_log.txt` file generated in the program folder.

## Technical Reference
For a complete technical analysis of the PKG and PFS decryption process, data structures, and cryptographic workflow, see the paper:

**[Technical Analysis of the Decryption Process for PlayStation 4 PKG and PFS File Formats](HOWWORKS.md)**

## Credits
- Based on reverse engineering from the PS4 scene
- Developed by seregonwar 
- License: GPL-2.0-or-later 
