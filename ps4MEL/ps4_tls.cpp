/*
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║                    PS4 TLS - IMPLEMENTATION                               ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║  Cross-platform TLS implementation based on shadPS4 patterns.             ║
 * ╚═══════════════════════════════════════════════════════════════════════════╝
 */

#include "ps4_tls.h"
#include <cstring>
#include <iostream>
#include <mutex>

#ifdef _WIN32
#include <windows.h>
#elif defined(__APPLE__) || defined(__linux__)
#include <pthread.h>
#endif

namespace PS4Emu {

/*
 * ┌─────────────────────────────────────────────────────────────────┐
 * │                    PLATFORM-SPECIFIC TLS                        │
 * └─────────────────────────────────────────────────────────────────┘
 */

#ifdef _WIN32
// ═══════════════════════════════════════════════════════════════════
// WINDOWS: Use TlsAlloc/TlsSetValue
// ═══════════════════════════════════════════════════════════════════
static DWORD g_tls_slot = 0;
static bool g_tls_initialized = false;

bool InitializeTLS() {
  if (g_tls_initialized)
    return true;

  g_tls_slot = TlsAlloc();
  if (g_tls_slot == TLS_OUT_OF_INDEXES) {
    std::cerr << "[PS4Emu] ERROR: Failed to allocate TLS slot" << std::endl;
    return false;
  }

  g_tls_initialized = true;
  std::cout << "[PS4Emu] TLS initialized (Windows, slot=" << g_tls_slot << ")"
            << std::endl;
  return true;
}

void ShutdownTLS() {
  if (g_tls_initialized) {
    TlsFree(g_tls_slot);
    g_tls_initialized = false;
  }
}

void SetTcbBase(Tcb *tcb) { TlsSetValue(g_tls_slot, tcb); }

Tcb *GetTcbBase() { return static_cast<Tcb *>(TlsGetValue(g_tls_slot)); }

#else
// ═══════════════════════════════════════════════════════════════════
// POSIX (macOS/Linux): Use pthread_key_t
// ═══════════════════════════════════════════════════════════════════
static pthread_key_t g_tls_key;
static bool g_tls_initialized = false;
static std::once_flag g_tls_init_flag;

static void TcbDestructor(void *tcb) {
  // Called when thread exits - we don't free TCB here as it may be managed
  // elsewhere
}

bool InitializeTLS() {
  bool success = false;
  std::call_once(g_tls_init_flag, [&success]() {
    if (pthread_key_create(&g_tls_key, TcbDestructor) != 0) {
      std::cerr << "[PS4Emu] ERROR: Failed to create TLS key" << std::endl;
      success = false;
      return;
    }
    g_tls_initialized = true;
    success = true;
    std::cout << "[PS4Emu] TLS initialized (POSIX)" << std::endl;
  });
  return g_tls_initialized;
}

void ShutdownTLS() {
  if (g_tls_initialized) {
    pthread_key_delete(g_tls_key);
    g_tls_initialized = false;
  }
}

void SetTcbBase(Tcb *tcb) { pthread_setspecific(g_tls_key, tcb); }

Tcb *GetTcbBase() { return static_cast<Tcb *>(pthread_getspecific(g_tls_key)); }

#endif

/*
 * ┌─────────────────────────────────────────────────────────────────┐
 * │                    COMMON IMPLEMENTATION                        │
 * └─────────────────────────────────────────────────────────────────┘
 */

Tcb *CreateTcb() {
  Tcb *tcb = new Tcb();
  std::memset(tcb, 0, sizeof(Tcb));

  // Self-pointer for FS/GS base access patterns
  tcb->tcb_self = tcb;
  tcb->tcb_dtv = nullptr;
  tcb->tcb_thread = nullptr;
  tcb->tcb_errno = 0;

  return tcb;
}

void DestroyTcb(Tcb *tcb) { delete tcb; }

int32_t *GetErrno() {
  Tcb *tcb = GetTcbBase();
  if (!tcb) {
    // Fallback: create TCB if not exists
    static thread_local int32_t fallback_errno = 0;
    return &fallback_errno;
  }
  return &tcb->tcb_errno;
}

} // namespace PS4Emu
