/*
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║                      PS4 TLS (Thread Local Storage)                       ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║  Emulates PS4's Thread Control Block for cross-platform compatibility.   ║
 * ║  Based on shadPS4 implementation patterns.                                ║
 * ╚═══════════════════════════════════════════════════════════════════════════╝
 */

#pragma once

#include <cstdint>

namespace PS4Emu {

/*
 * ┌─────────────────────────────────────────────────────────────────┐
 * │                  THREAD CONTROL BLOCK (TCB)                     │
 * │                                                                 │
 * │  PS4 Layout (simplified):                                       │
 * │  ┌──────────┬──────────┬──────────┬──────────┐                 │
 * │  │ self ptr │ dtv ptr  │ reserved │ errno    │                 │
 * │  └──────────┴──────────┴──────────┴──────────┘                 │
 * └─────────────────────────────────────────────────────────────────┘
 */
struct Tcb {
  Tcb *tcb_self;            // Pointer to self (for FS/GS base access)
  void *tcb_dtv;            // Dynamic Thread Vector for TLS
  void *tcb_thread;         // Current thread pointer
  int32_t tcb_errno;        // Thread-local errno
  uint64_t tcb_reserved[4]; // Reserved space

  // Padding to match PS4's TCB size
  uint8_t padding[256 - 56];
};

/*
 * ┌─────────────────────────────────────────────────────────────────┐
 * │                      PUBLIC API                                  │
 * └─────────────────────────────────────────────────────────────────┘
 */

// Initialize TLS subsystem (call once at startup)
bool InitializeTLS();

// Shutdown TLS subsystem
void ShutdownTLS();

// Set the current thread's TCB base pointer
void SetTcbBase(Tcb *tcb);

// Get the current thread's TCB base pointer
Tcb *GetTcbBase();

// Create a new TCB for a thread
Tcb *CreateTcb();

// Destroy a TCB
void DestroyTcb(Tcb *tcb);

// Get thread-local errno (for PS4 syscall emulation)
int32_t *GetErrno();

} // namespace PS4Emu
