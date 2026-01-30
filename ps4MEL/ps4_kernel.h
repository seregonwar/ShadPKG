/*
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║                     PS4 KERNEL STUBS - HEADER                             ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║  Emulates sceKernel* syscalls for PS4 compatibility.                      ║
 * ║  Based on shadPS4 implementation patterns.                                ║
 * ╚═══════════════════════════════════════════════════════════════════════════╝
 */

#pragma once

#include <cstdint>

/*
 * ┌─────────────────────────────────────────────────────────────────┐
 * │                      ERROR CODES                                │
 * │  PS4 uses ORBIS_* error codes (negative values)                │
 * └─────────────────────────────────────────────────────────────────┘
 */
#define ORBIS_OK 0
#define ORBIS_KERNEL_ERROR_UNKNOWN 0x80020000
#define ORBIS_KERNEL_ERROR_EPERM 0x80020001
#define ORBIS_KERNEL_ERROR_ENOENT 0x80020002
#define ORBIS_KERNEL_ERROR_ESRCH 0x80020003
#define ORBIS_KERNEL_ERROR_EINTR 0x80020004
#define ORBIS_KERNEL_ERROR_EIO 0x80020005
#define ORBIS_KERNEL_ERROR_ENOMEM 0x8002000C
#define ORBIS_KERNEL_ERROR_EACCES 0x8002000D
#define ORBIS_KERNEL_ERROR_EFAULT 0x8002000E
#define ORBIS_KERNEL_ERROR_EBUSY 0x80020010
#define ORBIS_KERNEL_ERROR_EEXIST 0x80020011
#define ORBIS_KERNEL_ERROR_EINVAL 0x80020016
#define ORBIS_KERNEL_ERROR_ENOSPC 0x8002001C
#define ORBIS_KERNEL_ERROR_EAGAIN 0x80020023
#define ORBIS_KERNEL_ERROR_ETIMEDOUT 0x8002003C

/*
 * ┌─────────────────────────────────────────────────────────────────┐
 * │                    MEMORY CONSTANTS                             │
 * └─────────────────────────────────────────────────────────────────┘
 */
constexpr uint64_t ORBIS_KERNEL_TOTAL_MEM = 5248ULL * 1024 * 1024; // 5248 MB

// Memory protection flags
#define ORBIS_KERNEL_PROT_CPU_READ 0x01
#define ORBIS_KERNEL_PROT_CPU_WRITE 0x02
#define ORBIS_KERNEL_PROT_CPU_RW 0x02
#define ORBIS_KERNEL_PROT_GPU_READ 0x10
#define ORBIS_KERNEL_PROT_GPU_WRITE 0x20
#define ORBIS_KERNEL_PROT_GPU_RW 0x30

// Memory types
#define ORBIS_KERNEL_WB_ONION 0   // Write-back (Onion bus)
#define ORBIS_KERNEL_WC_GARLIC 3  // Write-combining (Garlic bus)
#define ORBIS_KERNEL_WB_GARLIC 10 // Write-back (Garlic bus)

/*
 * ┌─────────────────────────────────────────────────────────────────┐
 * │                    STRUCTURES                                   │
 * └─────────────────────────────────────────────────────────────────┘
 */

struct OrbisKernelTimespec {
  int64_t tv_sec;
  int64_t tv_nsec;
};

struct OrbisKernelTimeval {
  int64_t tv_sec;
  int64_t tv_usec;
};

struct OrbisKernelStat {
  uint32_t st_dev;
  uint32_t st_ino;
  uint16_t st_mode;
  uint16_t st_nlink;
  uint32_t st_uid;
  uint32_t st_gid;
  uint32_t st_rdev;
  OrbisKernelTimespec st_atim;
  OrbisKernelTimespec st_mtim;
  OrbisKernelTimespec st_ctim;
  int64_t st_size;
  int64_t st_blocks;
  uint32_t st_blksize;
  uint32_t st_flags;
  uint32_t st_gen;
  int32_t st_lspare;
  OrbisKernelTimespec st_birthtim;
  uint8_t padding[8];
};

/*
 * ┌─────────────────────────────────────────────────────────────────┐
 * │                    SYSCALL DECLARATIONS                         │
 * └─────────────────────────────────────────────────────────────────┘
 */

extern "C" {

// ═══════════════════════════════════════════════════════════════════
// MEMORY SYSCALLS
// ═══════════════════════════════════════════════════════════════════
uint64_t sceKernelGetDirectMemorySize();
int32_t sceKernelAllocateDirectMemory(int64_t searchStart, int64_t searchEnd,
                                      uint64_t len, uint64_t alignment,
                                      int32_t memoryType, int64_t *physAddrOut);
int32_t sceKernelMapDirectMemory(void **addr, uint64_t len, int32_t prot,
                                 int32_t flags, int64_t physAddr,
                                 uint64_t alignment);
int32_t sceKernelMapFlexibleMemory(void **addrInOut, uint64_t len, int32_t prot,
                                   int32_t flags);
int32_t sceKernelMunmap(void *addr, uint64_t len);
int32_t sceKernelMprotect(const void *addr, uint64_t size, int32_t prot);

// ═══════════════════════════════════════════════════════════════════
// FILE SYSCALLS
// ═══════════════════════════════════════════════════════════════════
int32_t sceKernelOpen(const char *path, int32_t flags, uint16_t mode);
int32_t sceKernelClose(int32_t fd);
int64_t sceKernelRead(int32_t fd, void *buf, uint64_t nbytes);
int64_t sceKernelWrite(int32_t fd, const void *buf, uint64_t nbytes);
int64_t sceKernelLseek(int32_t fd, int64_t offset, int32_t whence);
int32_t sceKernelStat(const char *path, OrbisKernelStat *sb);
int32_t sceKernelFstat(int32_t fd, OrbisKernelStat *sb);
int32_t sceKernelMkdir(const char *path, uint16_t mode);

// ═══════════════════════════════════════════════════════════════════
// TIME SYSCALLS
// ═══════════════════════════════════════════════════════════════════
uint64_t sceKernelGetProcessTime();
uint64_t sceKernelGetProcessTimeCounter();
uint64_t sceKernelGetProcessTimeCounterFrequency();
int32_t sceKernelGettimeofday(OrbisKernelTimeval *tv);
int32_t sceKernelClockGettime(int32_t clockId, OrbisKernelTimespec *tp);
int32_t sceKernelUsleep(uint32_t microseconds);
int32_t sceKernelNanosleep(const OrbisKernelTimespec *rqtp,
                           OrbisKernelTimespec *rmtp);

// ═══════════════════════════════════════════════════════════════════
// PROCESS SYSCALLS
// ═══════════════════════════════════════════════════════════════════
int32_t sceKernelGetCurrentCpu();
int32_t sceKernelGetProcessId();

// ═══════════════════════════════════════════════════════════════════
// ERROR HANDLING
// ═══════════════════════════════════════════════════════════════════
int32_t *__Error(); // Returns pointer to thread-local errno

} // extern "C"
