/*
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║                        PS4 MOCK RUNTIME LAYER                             ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║  Complete runtime emulation for decompiled PS4 games.                     ║
 * ║  Includes: Memory, VFS, Syscalls, Threading stubs.                        ║
 * ╚═══════════════════════════════════════════════════════════════════════════╝
 */

#include "runtime.h"
#include "ps4_memory.h"
#include "ps4_tls.h"
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>

/*
 * ┌─────────────────────────────────────────────────────────────────┐
 * │                     VFS - VIRTUAL FILE SYSTEM                   │
 * └─────────────────────────────────────────────────────────────────┘
 * Translates PS4 paths to host filesystem paths.
 *
 *   /app0/      → assets/        (Game installation)
 *   /data/      → savedata/      (Save game data)
 *   /host/      → debug/         (Debug output)
 *   /download0/ → dlc/           (DLC content)
 */

static std::filesystem::path g_assetsRoot;

std::string resolve_path(const std::string &ps4_path) {
  std::string path = ps4_path;

  // Remove leading slash if present
  if (!path.empty() && path[0] == '/') {
    path = path.substr(1);
  }

  // Mount point translation
  if (path.find("app0/") == 0) {
    return (g_assetsRoot / path.substr(5)).string();
  }
  if (path.find("data/") == 0) {
    return (g_assetsRoot.parent_path() / "savedata" / path.substr(5)).string();
  }
  if (path.find("host/") == 0) {
    return (g_assetsRoot.parent_path() / "debug" / path.substr(5)).string();
  }
  if (path.find("download0/") == 0) {
    return (g_assetsRoot.parent_path() / "dlc" / path.substr(10)).string();
  }

  // Default: assume it's relative to assets
  return (g_assetsRoot / path).string();
}

/*
 * ┌─────────────────────────────────────────────────────────────────┐
 * │                    FILE DESCRIPTOR TABLE                        │
 * └─────────────────────────────────────────────────────────────────┘
 * Tracks open files for sceKernel* syscalls.
 */

static std::map<int, std::fstream *> g_openFiles;
static int g_nextFd = 100; // Start at 100 to avoid conflicts

/*
 * ┌─────────────────────────────────────────────────────────────────┐
 * │                     RUNTIME INITIALIZATION                      │
 * └─────────────────────────────────────────────────────────────────┘
 */

void runtime_init() {
  std::cout << "[RUNTIME] ══════════════════════════════════════════════"
            << std::endl;
  std::cout << "[RUNTIME] Initializing PS4 Mock Runtime..." << std::endl;

  // Initialize TLS
  if (!PS4Emu::InitializeTLS()) {
    std::cerr << "[RUNTIME] FATAL: Failed to initialize TLS!" << std::endl;
    std::exit(1);
  }

  // Create main thread TCB
  PS4Emu::Tcb *mainTcb = PS4Emu::CreateTcb();
  PS4Emu::SetTcbBase(mainTcb);
  std::cout << "[RUNTIME] Main thread TCB initialized" << std::endl;

  // Initialize memory emulation
  if (!PS4Emu::InitializeMemory()) {
    std::cerr << "[RUNTIME] FATAL: Failed to initialize memory emulation!"
              << std::endl;
    std::exit(1);
  }

  // Set up VFS root
  g_assetsRoot = std::filesystem::current_path() / "assets";
  std::cout << "[RUNTIME] VFS Root: \"" << g_assetsRoot.string() << "\""
            << std::endl;

  // Create required directories
  std::filesystem::create_directories(g_assetsRoot);
  std::filesystem::create_directories(g_assetsRoot.parent_path() / "savedata");
  std::filesystem::create_directories(g_assetsRoot.parent_path() / "debug");

  std::cout << "[RUNTIME] ══════════════════════════════════════════════"
            << std::endl;
}

void runtime_shutdown() {
  std::cout << "[RUNTIME] Shutting down..." << std::endl;

  // Close all open files
  for (auto &[fd, file] : g_openFiles) {
    if (file) {
      file->close();
      delete file;
    }
  }
  g_openFiles.clear();

  // Shutdown memory
  PS4Emu::ShutdownMemory();
}

/*
 * ┌─────────────────────────────────────────────────────────────────┐
 * │                     PS4 KERNEL SYSCALLS                         │
 * └─────────────────────────────────────────────────────────────────┘
 * Note: Most syscalls are now implemented in:
 *   - ps4_kernel.cpp (file, memory, time syscalls)
 *   - ps4_pthread.cpp (threading primitives)
 *   - ps4_stubs.cpp (PAD, Audio, Video, User Service, etc.)
 */

extern "C" {

// ─────────────────────── MEMORY (ADDITIONAL) ───────────────────────

void *sceKernelMmap(void *addr, size_t len, int prot, int flags, int fd,
                    off_t offset) {
  std::cout << "[SYSCALL] sceKernelMmap(len=" << len
            << ") -> using PS4Emu::TranslateAddress" << std::endl;
  return PS4Emu::TranslateAddress(reinterpret_cast<uint64_t>(addr));
}

// ─────────────────────── MODULES ───────────────────────

int sceKernelLoadStartModule(const char *path, size_t args, const void *argp,
                             unsigned int flags, void *option, int *result) {
  std::cout << "[SYSCALL] sceKernelLoadStartModule(\"" << path << "\") -> STUB"
            << std::endl;
  if (result)
    *result = 0;
  return 1;
}

} // extern "C"
