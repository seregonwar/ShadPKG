# shadPKG

[![Status](https://imrf.vercel.app/api/badge?label=Status&status=Active&color=10b981)]()
[![Platform](https://imrf.vercel.app/api/badge?label=Platform&status=Windows&color=0078d4)]()
[![License](https://imrf.vercel.app/api/badge?label=License&status=MIT&color=f59e0b)]()
<img src="https://img.shields.io/github/downloads/seregonwar/ShadPKG/total?style=flat-square&color=success" alt="Downloads"/><br/>


**shadPKG** is a C++ tool designed to derive encryption keys and extract content from PlayStation 4 PKG files.

---

## Overview

**shadPKG** is an advanced utility for analyzing, decrypting, and extracting PS4 PKG files, including games, updates, patches, DLCs, and homebrew packages.

It focuses on correctness, performance, and transparency, providing detailed logs and clear insight into the internal structure of PKG archives.

### Key capabilities:

* Full PKG structure analysis
* Automatic derivation and handling of encryption keys
* Decryption of protected data
* Complete file and directory extraction
* Support for retail, update, DLC, and homebrew PKGs
* Multi-threaded extraction with progress reporting

---

## ☕ Support Development

ShadPKG is actively maintained and improved over time.
If you find it useful and want to support continued development, you can contribute via Ko-fi.

[![Support me on Ko-fi](https://ko-fi.com/img/githubbutton_sm.svg)](https://ko-fi.com/seregon)

---

## System Requirements

* **OS:** Windows 10 / 11 (x64)
* **Compiler:** Visual Studio 2022 (Build Tools or full IDE)
* **Python:** 3.10 or newer (required for `build.py`)
* **Dependencies:** Conan 2.x

  * Used for managing libraries such as Zlib, Crypto++, and others

---

## Build Instructions

### 1. Clone the repository

```bash
git clone https://github.com/seregonwar/ShadPKG.git
cd ShadPKG
```

### 2. Install Conan (if missing)

```bash
pip install conan
```

### 3. Install dependencies

```bash
conan install . --build=missing
```

### 4. Build the project

Run the build script from the project root:

```bash
python build.py
```

The compiled binaries will be located in:

```
build/Release/
```

---

## Usage

From Command Prompt or PowerShell:

```text
shadPKG.exe <path_to_pkg> <output_directory>
```

### Example:

```text
shadPKG.exe "C:\GAMES\CUSA12345.pkg" C:\extracted\CUSA12345
```

### Behavior:

* All files and directories contained in the PKG are extracted to the target folder
* Progress is displayed in real time
* A detailed execution log is printed to the console and saved to `debug_log.txt`
* Unknown or unnamed entries are exported as:

  ```
  entry_0x<ID>.bin
  ```

---

## Main Features

* Multi-threaded extraction for improved performance
* Automatic key handling and decryption
* Robust support for different PKG types
* Persistent logging for debugging and analysis
* Safe path handling and error resilience

---

## Notes & Limitations

* Some PKG types (especially patches or updates) may intentionally omit certain files
* If extraction issues occur, consult the generated `debug_log.txt` before reporting problems

---

## Technical Documentation

A detailed breakdown of the PKG and PFS formats, including cryptographic flow, data structures, and decryption logic, is available here:

**📄 [Technical Analysis of the Decryption Process for PlayStation 4 PKG and PFS File Formats](docs/HOWWORKS.md)**

---

## Credits & License

* Based on reverse engineering and community documentation.
* Licensed under MIT.
