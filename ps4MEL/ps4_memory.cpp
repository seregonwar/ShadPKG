/*
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║                   PS4 MEMORY EMULATION - IMPLEMENTATION                   ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║  Uses mmap to allocate a region that simulates PS4 global address space.  ║
 * ╚═══════════════════════════════════════════════════════════════════════════╝
 */

#include "ps4_memory.h"
#include <cmath>
#include <cstring>
#include <iostream>

#ifdef __APPLE__
#include <mach/mach.h>
#include <sys/mman.h>
#elif defined(__linux__)
#include <sys/mman.h>
#elif defined(_WIN32)
#include <windows.h>
#endif

namespace PS4Emu {

// Global memory block pointer
static uint8_t *g_ps4GlobalMemory = nullptr;
static bool g_initialized = false;

/*
 * ┌─────────────────────────────────────────────────────────────────┐
 * │                    PLATFORM-SPECIFIC MMAP                       │
 * └─────────────────────────────────────────────────────────────────┘
 */

static void *AllocateMemoryBlock(size_t size) {
#if defined(__APPLE__) || defined(__linux__)
  // Use mmap with MAP_ANONYMOUS for a zeroed memory block
  void *ptr = mmap(nullptr, // Let OS choose address
                   size,
                   PROT_READ | PROT_WRITE,      // Read/write access
                   MAP_PRIVATE | MAP_ANONYMOUS, // Private anonymous mapping
                   -1,                          // No file descriptor
                   0                            // No offset
  );

  if (ptr == MAP_FAILED) {
    std::cerr << "[PS4Emu] ERROR: mmap failed for global memory!" << std::endl;
    return nullptr;
  }
  return ptr;

#elif defined(_WIN32)
  void *ptr =
      VirtualAlloc(nullptr, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
  if (!ptr) {
    std::cerr << "[PS4Emu] ERROR: VirtualAlloc failed for global memory!"
              << std::endl;
  }
  return ptr;
#else
#error "Unsupported platform"
#endif
}

static void FreeMemoryBlock(void *ptr, size_t size) {
#if defined(__APPLE__) || defined(__linux__)
  munmap(ptr, size);
#elif defined(_WIN32)
  VirtualFree(ptr, 0, MEM_RELEASE);
#endif
}

/*
 * ┌─────────────────────────────────────────────────────────────────┐
 * │                      PUBLIC API IMPLEMENTATION                  │
 * └─────────────────────────────────────────────────────────────────┘
 */

bool InitializeMemory() {
  if (g_initialized) {
    std::cerr << "[PS4Emu] Warning: Memory already initialized" << std::endl;
    return true;
  }

  std::cout << "[PS4Emu] Initializing PS4 global memory emulation..."
            << std::endl;
  std::cout << "[PS4Emu] Allocating " << (PS4_GLOBAL_SIZE / 1024 / 1024)
            << "MB for globals" << std::endl;

  g_ps4GlobalMemory =
      static_cast<uint8_t *>(AllocateMemoryBlock(PS4_GLOBAL_SIZE));

  if (!g_ps4GlobalMemory) {
    return false;
  }

  // Zero-initialize the entire block
  std::memset(g_ps4GlobalMemory, 0, PS4_GLOBAL_SIZE);

  g_initialized = true;
  std::cout << "[PS4Emu] Global memory ready at host address: "
            << static_cast<void *>(g_ps4GlobalMemory) << std::endl;

  // ┌─────────────────────────────────────────────────────────────────────────┐
  // │  INITIALIZE CRITICAL GLOBAL STUBS                                       │
  // │  These addresses are heavily accessed and need valid pointers           │
  // └─────────────────────────────────────────────────────────────────────────┘

  // Create a "safe zone" for dummy objects at the end of our region
  // Each dummy object is 256 bytes to hold a minimal VTable-like structure
  constexpr size_t DUMMY_OBJECT_SIZE = 256;
  constexpr size_t SAFE_ZONE_START =
      PS4_GLOBAL_SIZE - (1024 * 1024); // Last 1MB
  static size_t nextDummyOffset = SAFE_ZONE_START;

  auto allocateDummy = [&]() -> uint64_t {
    uint64_t offset = nextDummyOffset;
    nextDummyOffset += DUMMY_OBJECT_SIZE;

    // Get the HOST address of this dummy object
    uint8_t *hostAddr = g_ps4GlobalMemory + offset;
    uint64_t hostAddrValue = reinterpret_cast<uint64_t>(hostAddr);

    // Initialize the dummy with HOST pointers to itself
    // This allows chained dereferences to work correctly
    uint64_t *ptr = reinterpret_cast<uint64_t *>(hostAddr);
    for (size_t i = 0; i < DUMMY_OBJECT_SIZE / sizeof(uint64_t); i++) {
      ptr[i] = hostAddrValue; // Each slot points back to this same object (host addr)
    }

    return hostAddrValue; // Return the HOST address, not the offset
  };

  // List of critical globals that need initialization
  // Format: {address, description}
  struct GlobalInit {
    uint64_t addr;
    const char *desc;
  };

  GlobalInit criticalGlobals[] = {
      {0x995e78, "Main game context (36k refs)"},
      {0x9bb298, "Object pool (4k refs)"},
      {0xa21da8, "System table A"},
      {0xa21d98, "System table B"},
      {0x949e38, "Runtime state"},
      {0xa21d88, "System table C"},
      {0x995e60, "Engine pointer A"},
      {0x995e68, "Engine pointer B"},
      {0xa21d90, "System table D"},
      {0xa21d80, "System table E"},
      {0xa21da0, "System table F"},
      {0x925500, "Resource manager A"},
      {0x9254b0, "Resource manager B"},
      {0x925420, "Resource manager C"},
      {0x9d00d4, "Config value"},
      {0x9bc3d0, "Memory allocator"},
      {0x99a9e8, "Scene manager"},
      {0x9992b0, "Render context"},
      {0x96d060, "Audio manager"},
      {0x959900, "Input manager"},
      {0x9d9e28, "TLS context"}, // The one causing the first crash
      {0x9d9e20, "TLS pointer A"},
      {0x9d9e22, "TLS pointer B"},
      {0x961800, "Module table A"},
      {0x961850, "Module table B"},
      {0x995e40, "Global State A"},
      {0x997420, "Global State B"},
      {0x997438, "Global State C"},
      {0x99743a, "Global State D"},
      {0x997430, "Global State E"},
      // Additional managers discovered during runtime
      {0x9bb2a0, "Object Pool B"},
      {0x9bb2a8, "Object Pool C"},
      {0x99a9f0, "Scene Manager B"},
      {0x99a9f8, "Scene Manager C"},
      {0x9992b8, "Render Context B"},
      {0x9992c0, "Render Context C"},
      {0x96d068, "Audio Manager B"},
      {0x96d070, "Audio Manager C"},
      {0x959908, "Input Manager B"},
      {0x959910, "Input Manager C"},
      {0x959918, "Input State"},
      {0x9bc3d8, "Memory Allocator B"},
      {0x9bc3e0, "Memory Allocator C"},
      {0x9d00d8, "Config Value B"},
      {0x9d00e0, "Config Value C"},
      // Frame timing and loop control
      {0x9d9e30, "Frame Counter"},
      {0x9d9e38, "Delta Time Ptr"},
      {0x9d9e40, "Last Frame Time"},
      // Additional critical pointers from crash analysis
      {0x961858, "Module Table C"},
      {0x961860, "Module Table D"},
      {0x995e80, "Engine Pointer C"},
      {0x995e88, "Engine Pointer D"},
  };

  std::cout << "[PS4Emu] Initializing "
            << (sizeof(criticalGlobals) / sizeof(GlobalInit))
            << " critical global stubs..." << std::endl;

  for (const auto &g : criticalGlobals) {
    uint64_t dummyAddr = allocateDummy();
    // Write the dummy address into the global slot
    *reinterpret_cast<uint64_t *>(g_ps4GlobalMemory + g.addr) = dummyAddr;
  }

  std::cout << "[PS4Emu] Global stubs initialized successfully" << std::endl;

  // ┌─────────────────────────────────────────────────────────────────────────┐
  // │  INITIALIZE TIMING CONSTANTS                                            │
  // │  These are float values used in frame timing calculations               │
  // │  Without them, loops multiply by 0 forever                              │
  // └─────────────────────────────────────────────────────────────────────────┘

  // Frame timing values (addresses 0x7d69c4 - 0x7d69d4)
  // These appear to be reciprocals of loop iteration counts
  float timingConstants[] = {
      1.0f / 65536.0f, // 0x7d69c4 - reciprocal of 0x10000
      65536.0f,        // 0x7d69c8 - loop count
      1.0f / 65536.0f, // 0x7d69cc - reciprocal of 0x10000 (causes loop!)
      65536.0f,        // 0x7d69d0 - loop count
      0.5f,            // 0x7d69d4 - offset
  };

  uint32_t timingAddrs[] = {0x7d69c4, 0x7d69c8, 0x7d69cc, 0x7d69d0, 0x7d69d4};

  for (size_t i = 0; i < 5; i++) {
    *reinterpret_cast<float *>(g_ps4GlobalMemory + timingAddrs[i]) =
        timingConstants[i];
  }

  // Additional float constants found in the code
  *reinterpret_cast<float *>(g_ps4GlobalMemory + 0x7cb0f4) = 1.0f;
  *reinterpret_cast<float *>(g_ps4GlobalMemory + 0x7cb0f8) = 1.0f;
  *reinterpret_cast<float *>(g_ps4GlobalMemory + 0x7d6a50) = 1.0f;
  *reinterpret_cast<float *>(g_ps4GlobalMemory + 0x7d6a54) = 1.0f;
  *reinterpret_cast<float *>(g_ps4GlobalMemory + 0x7d6a58) = 1.0f;

  std::cout << "[PS4Emu] Timing constants initialized" << std::endl;

  // ┌─────────────────────────────────────────────────────────────────────────┐
  // │  PRE-POPULATE TRIG TABLES                                               │
  // │  The decompiler corrupted the loop that builds sine/cosine tables.      │
  // │  We pre-populate them here so the game skips the broken init code.      │
  // └─────────────────────────────────────────────────────────────────────────┘

  // Allocate trig table: 65536 entries * 4 bytes = 256KB
  constexpr size_t TRIG_TABLE_ENTRIES = 0x10000;
  constexpr size_t TRIG_TABLE_SIZE = TRIG_TABLE_ENTRIES * sizeof(float);
  constexpr size_t TRIG_TABLE_OFFSET = SAFE_ZONE_START - TRIG_TABLE_SIZE;

  float *trigTable =
      reinterpret_cast<float *>(g_ps4GlobalMemory + TRIG_TABLE_OFFSET);

  // Fill with sin/cos values (common game engine lookup table)
  for (size_t i = 0; i < TRIG_TABLE_ENTRIES; i++) {
    float angle = (float(i) / TRIG_TABLE_ENTRIES) * 2.0f * 3.14159265358979f;
    trigTable[i] = std::sin(angle);
  }

  // Store pointer to trig table at 0x9d9e28 (the address the loops check)
  // This makes reg_rax valid so the check passes and loops are skipped
  *reinterpret_cast<uint64_t *>(g_ps4GlobalMemory + 0x9d9e28) =
      reinterpret_cast<uint64_t>(trigTable);

  std::cout << "[PS4Emu] Trig tables pre-populated (" << TRIG_TABLE_ENTRIES
            << " entries)" << std::endl;

  // ┌─────────────────────────────────────────────────────────────────────────┐
  // │  INITIALIZE FRAME COUNTER FOR DEBUGGING                                 │
  // │  This allows us to track if the main loop is actually running           │
  // └─────────────────────────────────────────────────────────────────────────┘
  
  // Frame counter at a known location for debugging
  *reinterpret_cast<uint64_t *>(g_ps4GlobalMemory + 0x9d9e30) = 0;
  
  // Delta time initialized to 16.67ms (60 FPS)
  *reinterpret_cast<float *>(g_ps4GlobalMemory + 0x9d9e38) = 0.01667f;
  
  // Last frame time
  *reinterpret_cast<uint64_t *>(g_ps4GlobalMemory + 0x9d9e40) = 0;

  std::cout << "[PS4Emu] Frame counter initialized at 0x9d9e30" << std::endl;

  return true;
}

void ShutdownMemory() {
  if (g_ps4GlobalMemory) {
    std::cout << "[PS4Emu] Shutting down memory emulation..." << std::endl;
    FreeMemoryBlock(g_ps4GlobalMemory, PS4_GLOBAL_SIZE);
    g_ps4GlobalMemory = nullptr;
    g_initialized = false;
  }
}

void *TranslateAddress(uint64_t ps4_addr) {
  // ═══════════════════════════════════════════════════════════════════
  // LOOP DETECTION: Count calls and exit after limit
  // ═══════════════════════════════════════════════════════════════════
  static uint64_t s_call_count = 0;
  static constexpr uint64_t LOG_INTERVAL = 1000000; // Log every 1M calls
  static constexpr uint64_t CALL_LIMIT = 50000000;  // Exit after 50M calls

  s_call_count++;

  if (s_call_count % LOG_INTERVAL == 0) {
    std::cout << "[PS4Emu] TranslateAddress calls: " << s_call_count
              << " (last addr: 0x" << std::hex << ps4_addr << std::dec << ")"
              << std::endl;
  }

  if (s_call_count >= CALL_LIMIT) {
    std::cerr << "[PS4Emu] LIMIT REACHED: " << CALL_LIMIT
              << " memory accesses. Exiting to prevent hang." << std::endl;
    std::exit(1);
  }

  // Safety check: ensure memory is initialized
  if (!g_initialized || !g_ps4GlobalMemory) {
    std::cerr << "[PS4Emu] ERROR: Accessing global memory before "
                 "initialization! addr=0x"
              << std::hex << ps4_addr << std::endl;
    return nullptr;
  }

  // Check if address is within the PS4 global range
  if (ps4_addr >= PS4_GLOBAL_SIZE) {
    // Silent remapping - too noisy otherwise
    ps4_addr = ps4_addr % PS4_GLOBAL_SIZE;
  }

  // Translate: PS4 address -> our allocated block
  return g_ps4GlobalMemory + ps4_addr;
}

// ┌─────────────────────────────────────────────────────────────────────────┐
// │  DEBUG: Get frame counter to check if main loop is running              │
// └─────────────────────────────────────────────────────────────────────────┘
uint64_t GetFrameCounter() {
  if (!g_initialized || !g_ps4GlobalMemory) {
    return 0;
  }
  return *reinterpret_cast<uint64_t *>(g_ps4GlobalMemory + 0x9d9e30);
}

void IncrementFrameCounter() {
  if (!g_initialized || !g_ps4GlobalMemory) {
    return;
  }
  uint64_t *counter = reinterpret_cast<uint64_t *>(g_ps4GlobalMemory + 0x9d9e30);
  (*counter)++;
  
  // Log every 60 frames (approximately 1 second at 60 FPS)
  if (*counter % 60 == 0) {
    std::cout << "[PS4Emu] Frame: " << *counter << std::endl;
  }
}

void *GetGlobalMemoryBase() {
  return g_ps4GlobalMemory;
}

} // namespace PS4Emu
