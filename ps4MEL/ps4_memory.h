/*
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║                        PS4 MEMORY EMULATION LAYER                         ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║  Provides emulation of PS4 global memory space on host platforms.         ║
 * ║  The PS4 binary has hardcoded addresses that we need to make accessible.  ║
 * ╚═══════════════════════════════════════════════════════════════════════════╝
 */

#pragma once

#include <cstddef>
#include <cstdint>

namespace PS4Emu {

/*
 * ┌─────────────────────────────────────────────────────────────────┐
 * │                     MEMORY LAYOUT CONSTANTS                     │
 * └─────────────────────────────────────────────────────────────────┘
 * PS4 typical memory regions:
 *   0x00000000 - 0x00FFFFFF : Low memory (globals, TLS)
 *   0x00400000 - 0x00FFFFFF : Executable base (eboot.bin)
 *   0x01000000+             : Heap, libraries
 */

// Base address where we'll map PS4 global space
// We use a high address to avoid conflicts with host allocations
constexpr uint64_t PS4_GLOBAL_BASE = 0x100000000ULL;

// Size of the global memory region (16MB should cover most globals)
constexpr size_t PS4_GLOBAL_SIZE = 16 * 1024 * 1024;

// Maximum address we'll redirect (anything above this is likely heap)
constexpr uint64_t PS4_GLOBAL_MAX = 0x01000000ULL;

/*
 * ┌─────────────────────────────────────────────────────────────────┐
 * │                      INITIALIZATION API                         │
 * └─────────────────────────────────────────────────────────────────┘
 */

// Initialize the PS4 memory emulation layer
// Must be called before any game code runs
bool InitializeMemory();

// Cleanup the memory layer
void ShutdownMemory();

// Get the host pointer for a PS4 global address
// Returns nullptr if address is out of range
void *TranslateAddress(uint64_t ps4_addr);

// Debug: Frame counter functions to verify main loop execution
uint64_t GetFrameCounter();
void IncrementFrameCounter();
void *GetGlobalMemoryBase();

/*
 * ┌─────────────────────────────────────────────────────────────────┐
 * │                       ACCESS MACROS                             │
 * └─────────────────────────────────────────────────────────────────┘
 * These macros provide safe access to PS4 global memory.
 * Example: instead of *(int64_t*)0x9d9e28
 *          use PS4_GLOBAL_I64(0x9d9e28)
 */

#define PS4_GLOBAL_PTR(addr) (PS4Emu::TranslateAddress(addr))

#define PS4_GLOBAL_I8(addr) (*reinterpret_cast<int8_t *>(PS4_GLOBAL_PTR(addr)))

#define PS4_GLOBAL_I16(addr)                                                   \
  (*reinterpret_cast<int16_t *>(PS4_GLOBAL_PTR(addr)))

#define PS4_GLOBAL_I32(addr)                                                   \
  (*reinterpret_cast<int32_t *>(PS4_GLOBAL_PTR(addr)))

#define PS4_GLOBAL_I64(addr)                                                   \
  (*reinterpret_cast<int64_t *>(PS4_GLOBAL_PTR(addr)))

#define PS4_GLOBAL_U8(addr) (*reinterpret_cast<uint8_t *>(PS4_GLOBAL_PTR(addr)))

#define PS4_GLOBAL_U16(addr)                                                   \
  (*reinterpret_cast<uint16_t *>(PS4_GLOBAL_PTR(addr)))

#define PS4_GLOBAL_U32(addr)                                                   \
  (*reinterpret_cast<uint32_t *>(PS4_GLOBAL_PTR(addr)))

#define PS4_GLOBAL_U64(addr)                                                   \
  (*reinterpret_cast<uint64_t *>(PS4_GLOBAL_PTR(addr)))

} // namespace PS4Emu
