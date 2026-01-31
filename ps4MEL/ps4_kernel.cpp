/*
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║                   PS4 KERNEL STUBS - IMPLEMENTATION                       ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║  Emulates sceKernel* syscalls with logging for debugging.                 ║
 * ╚═══════════════════════════════════════════════════════════════════════════╝
 */

#include "ps4_kernel.h"
#include "ps4_memory.h"
#include "ps4_tls.h"
#include <chrono>
#include <cstring>
#include <ctime>
#include <fcntl.h>
#include <iomanip>
#include <iostream>
#include <map>
#include <mutex>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

// VFS path prefix
static const char *g_vfs_root = nullptr;

/*
 * ┌─────────────────────────────────────────────────────────────────┐
 * │                    LOGGING HELPERS                              │
 * └─────────────────────────────────────────────────────────────────┘
 */
#define LOG_STUB(name)                                                         \
  std::cout << "[PS4Kernel] " << name << "() called" << std::endl

#define LOG_STUB_ARGS(name, args)                                              \
  std::cout << "[PS4Kernel] " << name << "(" << args << ") called" << std::endl

/*
 * ┌─────────────────────────────────────────────────────────────────┐
 * │                    FILE DESCRIPTOR TABLE                        │
 * └─────────────────────────────────────────────────────────────────┘
 */
static std::map<int32_t, int> g_fd_map; // PS4 fd -> host fd
static int32_t g_next_fd = 3;           // Start after stdin/stdout/stderr
static std::mutex g_fd_mutex;

static int32_t AllocateFd(int hostFd) {
  std::lock_guard<std::mutex> lock(g_fd_mutex);
  int32_t ps4Fd = g_next_fd++;
  g_fd_map[ps4Fd] = hostFd;
  return ps4Fd;
}

static int GetHostFd(int32_t ps4Fd) {
  std::lock_guard<std::mutex> lock(g_fd_mutex);
  auto it = g_fd_map.find(ps4Fd);
  if (it == g_fd_map.end())
    return -1;
  return it->second;
}

static void FreeFd(int32_t ps4Fd) {
  std::lock_guard<std::mutex> lock(g_fd_mutex);
  g_fd_map.erase(ps4Fd);
}

/*
 * ┌─────────────────────────────────────────────────────────────────┐
 * │                    VFS PATH TRANSLATION                         │
 * │                                                                 │
 * │  /app0/  → assets/                                              │
 * │  /data/  → savedata/                                            │
 * │  /host0/ → current directory                                    │
 * └─────────────────────────────────────────────────────────────────┘
 */
static std::string TranslatePath(const char *ps4Path) {
  if (!ps4Path)
    return "";

  std::string path(ps4Path);
  std::string basePath = g_vfs_root ? g_vfs_root : "assets";

  // Remove leading slash if present
  if (!path.empty() && path[0] == '/') {
    // Check for known prefixes
    if (path.rfind("/app0/", 0) == 0) {
      return basePath + "/" + path.substr(6);
    }
    if (path.rfind("/data/", 0) == 0) {
      return "savedata/" + path.substr(6);
    }
    if (path.rfind("/host0/", 0) == 0) {
      return path.substr(7);
    }
    // Default: strip leading slash
    return basePath + path;
  }

  return basePath + "/" + path;
}

extern "C" {

/*
 * ┌─────────────────────────────────────────────────────────────────┐
 * │                    MEMORY SYSCALLS                              │
 * └─────────────────────────────────────────────────────────────────┘
 */

uint64_t sceKernelGetDirectMemorySize() {
  LOG_STUB("sceKernelGetDirectMemorySize");
  return ORBIS_KERNEL_TOTAL_MEM;
}

int32_t sceKernelAllocateDirectMemory(int64_t searchStart, int64_t searchEnd,
                                      uint64_t len, uint64_t alignment,
                                      int32_t memoryType,
                                      int64_t *physAddrOut) {
  LOG_STUB_ARGS("sceKernelAllocateDirectMemory",
                "len=" << len << ", alignment=" << alignment);

  // Use our existing PS4 memory system
  static int64_t nextPhysAddr = 0x100000; // Start at 1MB

  // Align the address
  if (alignment > 0) {
    nextPhysAddr = (nextPhysAddr + alignment - 1) & ~(alignment - 1);
  }

  if (physAddrOut) {
    *physAddrOut = nextPhysAddr;
  }

  nextPhysAddr += len;
  return ORBIS_OK;
}

int32_t sceKernelMapDirectMemory(void **addr, uint64_t len, int32_t prot,
                                 int32_t flags, int64_t physAddr,
                                 uint64_t alignment) {
  LOG_STUB_ARGS("sceKernelMapDirectMemory",
                "len=" << len << ", physAddr=0x" << std::hex << physAddr);

  // Allocate memory using standard mmap
  void *ptr = mmap(nullptr, len, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

  if (ptr == MAP_FAILED) {
    return ORBIS_KERNEL_ERROR_ENOMEM;
  }

  if (addr) {
    *addr = ptr;
  }

  return ORBIS_OK;
}

int32_t sceKernelMapFlexibleMemory(void **addrInOut, uint64_t len, int32_t prot,
                                   int32_t flags) {
  LOG_STUB_ARGS("sceKernelMapFlexibleMemory", "len=" << len);

  void *ptr = mmap(nullptr, len, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

  if (ptr == MAP_FAILED) {
    return ORBIS_KERNEL_ERROR_ENOMEM;
  }

  if (addrInOut) {
    *addrInOut = ptr;
  }

  return ORBIS_OK;
}

int32_t sceKernelMunmap(void *addr, uint64_t len) {
  LOG_STUB_ARGS("sceKernelMunmap", "addr=" << addr << ", len=" << len);

  if (munmap(addr, len) != 0) {
    return ORBIS_KERNEL_ERROR_EINVAL;
  }
  return ORBIS_OK;
}

int32_t sceKernelMprotect(const void *addr, uint64_t size, int32_t prot) {
  LOG_STUB_ARGS("sceKernelMprotect", "addr=" << addr << ", size=" << size);

  int hostProt = 0;
  if (prot & ORBIS_KERNEL_PROT_CPU_READ)
    hostProt |= PROT_READ;
  if (prot & ORBIS_KERNEL_PROT_CPU_WRITE)
    hostProt |= PROT_WRITE;

  if (mprotect(const_cast<void *>(addr), size, hostProt) != 0) {
    return ORBIS_KERNEL_ERROR_EINVAL;
  }
  return ORBIS_OK;
}

/*
 * ┌─────────────────────────────────────────────────────────────────┐
 * │                    FILE SYSCALLS                                │
 * └─────────────────────────────────────────────────────────────────┘
 */

int32_t sceKernelOpen(const char *path, int32_t flags, uint16_t mode) {
  std::string hostPath = TranslatePath(path);
  LOG_STUB_ARGS("sceKernelOpen",
                "path=\"" << path << "\" -> \"" << hostPath << "\"");

  // Convert PS4 flags to POSIX
  int hostFlags = 0;
  if (flags & 0x0001)
    hostFlags |= O_WRONLY;
  if (flags & 0x0002)
    hostFlags |= O_RDWR;
  if (flags & 0x0200)
    hostFlags |= O_CREAT;
  if (flags & 0x0400)
    hostFlags |= O_TRUNC;
  if (flags & 0x0008)
    hostFlags |= O_APPEND;
  if ((flags & 0x0003) == 0)
    hostFlags |= O_RDONLY;

  int hostFd = open(hostPath.c_str(), hostFlags, mode);
  if (hostFd < 0) {
    std::cerr << "[PS4Kernel] sceKernelOpen FAILED: " << hostPath << std::endl;
    return ORBIS_KERNEL_ERROR_ENOENT;
  }

  return AllocateFd(hostFd);
}

int32_t sceKernelClose(int32_t fd) {
  LOG_STUB_ARGS("sceKernelClose", "fd=" << fd);

  int hostFd = GetHostFd(fd);
  if (hostFd < 0)
    return ORBIS_KERNEL_ERROR_EINVAL;

  close(hostFd);
  FreeFd(fd);
  return ORBIS_OK;
}

int64_t sceKernelRead(int32_t fd, void *buf, uint64_t nbytes) {
  int hostFd = GetHostFd(fd);
  if (hostFd < 0)
    return ORBIS_KERNEL_ERROR_EINVAL;

  ssize_t result = read(hostFd, buf, nbytes);
  if (result < 0)
    return ORBIS_KERNEL_ERROR_EIO;
  return result;
}

int64_t sceKernelWrite(int32_t fd, const void *buf, uint64_t nbytes) {
  int hostFd = GetHostFd(fd);
  if (hostFd < 0)
    return ORBIS_KERNEL_ERROR_EINVAL;

  ssize_t result = write(hostFd, buf, nbytes);
  if (result < 0)
    return ORBIS_KERNEL_ERROR_EIO;
  return result;
}

int64_t sceKernelLseek(int32_t fd, int64_t offset, int32_t whence) {
  int hostFd = GetHostFd(fd);
  if (hostFd < 0)
    return ORBIS_KERNEL_ERROR_EINVAL;

  off_t result = lseek(hostFd, offset, whence);
  if (result < 0)
    return ORBIS_KERNEL_ERROR_EINVAL;
  return result;
}

int32_t sceKernelStat(const char *path, OrbisKernelStat *sb) {
  std::string hostPath = TranslatePath(path);
  LOG_STUB_ARGS("sceKernelStat", "path=\"" << hostPath << "\"");

  struct stat hostStat;
  if (stat(hostPath.c_str(), &hostStat) != 0) {
    return ORBIS_KERNEL_ERROR_ENOENT;
  }

  memset(sb, 0, sizeof(OrbisKernelStat));
  sb->st_size = hostStat.st_size;
  sb->st_mode = hostStat.st_mode;
  return ORBIS_OK;
}

int32_t sceKernelFstat(int32_t fd, OrbisKernelStat *sb) {
  int hostFd = GetHostFd(fd);
  if (hostFd < 0)
    return ORBIS_KERNEL_ERROR_EINVAL;

  struct stat hostStat;
  if (fstat(hostFd, &hostStat) != 0) {
    return ORBIS_KERNEL_ERROR_EIO;
  }

  memset(sb, 0, sizeof(OrbisKernelStat));
  sb->st_size = hostStat.st_size;
  sb->st_mode = hostStat.st_mode;
  return ORBIS_OK;
}

int32_t sceKernelMkdir(const char *path, uint16_t mode) {
  std::string hostPath = TranslatePath(path);
  LOG_STUB_ARGS("sceKernelMkdir", "path=\"" << hostPath << "\"");

  if (mkdir(hostPath.c_str(), mode) != 0) {
    return ORBIS_KERNEL_ERROR_EEXIST;
  }
  return ORBIS_OK;
}

/*
 * ┌─────────────────────────────────────────────────────────────────┐
 * │                    TIME SYSCALLS                                │
 * └─────────────────────────────────────────────────────────────────┘
 */

uint64_t sceKernelGetProcessTime() {
  auto now = std::chrono::steady_clock::now();
  auto duration = now.time_since_epoch();
  return std::chrono::duration_cast<std::chrono::microseconds>(duration)
      .count();
}

uint64_t sceKernelGetProcessTimeCounter() { return sceKernelGetProcessTime(); }

uint64_t sceKernelGetProcessTimeCounterFrequency() {
  return 1000000; // 1 MHz (microsecond precision)
}

int32_t sceKernelGettimeofday(OrbisKernelTimeval *tv) {
  if (!tv)
    return ORBIS_KERNEL_ERROR_EINVAL;

  auto now = std::chrono::system_clock::now();
  auto duration = now.time_since_epoch();
  auto seconds = std::chrono::duration_cast<std::chrono::seconds>(duration);
  auto micros =
      std::chrono::duration_cast<std::chrono::microseconds>(duration - seconds);

  tv->tv_sec = seconds.count();
  tv->tv_usec = micros.count();
  return ORBIS_OK;
}

int32_t sceKernelClockGettime(int32_t clockId, OrbisKernelTimespec *tp) {
  if (!tp)
    return ORBIS_KERNEL_ERROR_EINVAL;

  auto now = std::chrono::steady_clock::now();
  auto duration = now.time_since_epoch();
  auto seconds = std::chrono::duration_cast<std::chrono::seconds>(duration);
  auto nanos =
      std::chrono::duration_cast<std::chrono::nanoseconds>(duration - seconds);

  tp->tv_sec = seconds.count();
  tp->tv_nsec = nanos.count();
  return ORBIS_OK;
}

int32_t sceKernelUsleep(uint32_t microseconds) {
  usleep(microseconds);
  return ORBIS_OK;
}

int32_t sceKernelNanosleep(const OrbisKernelTimespec *rqtp,
                           OrbisKernelTimespec *rmtp) {
  if (!rqtp)
    return ORBIS_KERNEL_ERROR_EINVAL;

  struct timespec req = {.tv_sec = static_cast<time_t>(rqtp->tv_sec),
                         .tv_nsec = static_cast<long>(rqtp->tv_nsec)};
  struct timespec rem;

  nanosleep(&req, &rem);

  if (rmtp) {
    rmtp->tv_sec = rem.tv_sec;
    rmtp->tv_nsec = rem.tv_nsec;
  }
  return ORBIS_OK;
}

/*
 * ┌─────────────────────────────────────────────────────────────────┐
 * │                    PROCESS SYSCALLS                             │
 * └─────────────────────────────────────────────────────────────────┘
 */

int32_t sceKernelGetCurrentCpu() {
  return 0; // Always return CPU 0
}

int32_t sceKernelGetProcessId() {
  return 1; // Fake PID
}

/*
 * ┌─────────────────────────────────────────────────────────────────┐
 * │                    ERROR HANDLING                               │
 * └─────────────────────────────────────────────────────────────────┘
 */

int32_t *__Error() { return PS4Emu::GetErrno(); }

} // extern "C"

/*
 * ┌─────────────────────────────────────────────────────────────────┐
 * │                    INITIALIZATION                               │
 * └─────────────────────────────────────────────────────────────────┘
 */
namespace PS4Emu {

void SetVfsRoot(const char *root) { g_vfs_root = root; }

} // namespace PS4Emu
