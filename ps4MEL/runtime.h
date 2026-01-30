/*
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║                        PS4 MOCK RUNTIME HEADER                            ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║  Declarations for PS4 syscall stubs and runtime functions.                ║
 * ╚═══════════════════════════════════════════════════════════════════════════╝
 */

#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>
#include <thread>

/*
 * ┌─────────────────────────────────────────────────────────────────┐
 * │                    RUNTIME LIFECYCLE                            │
 * └─────────────────────────────────────────────────────────────────┘
 */

void runtime_init();
void runtime_shutdown();
std::string resolve_path(const std::string &ps4_path);

/*
 * ┌─────────────────────────────────────────────────────────────────┐
 * │                    PS4 KERNEL SYSCALLS                          │
 * └─────────────────────────────────────────────────────────────────┘
 */

extern "C" {

// ─────────────────────── FILE I/O ───────────────────────
int sceKernelOpen(const char *path, int flags, int mode);
ssize_t sceKernelRead(int fd, void *buf, size_t nbyte);
int sceKernelClose(int fd);
off_t sceKernelLseek(int fd, off_t offset, int whence);

// ─────────────────────── MEMORY ───────────────────────
void *sceKernelMmap(void *addr, size_t len, int prot, int flags, int fd,
                    off_t offset);
int sceKernelMunmap(void *addr, size_t len);

// ─────────────────────── THREADING ───────────────────────
int scePthreadCreate(void *thread, void *attr, void *(*start_routine)(void *),
                     void *arg);
int scePthreadMutexLock(void *mutex);
int scePthreadMutexUnlock(void *mutex);

// ─────────────────────── MODULES ───────────────────────
int sceKernelLoadStartModule(const char *path, size_t args, const void *argp,
                             unsigned int flags, void *option, int *result);
int sceSysmoduleLoadModule(int id);

// ─────────────────────── MISC ───────────────────────
uint64_t sceKernelGetProcessTime();
int sceKernelUsleep(unsigned int usec);

} // extern "C"
