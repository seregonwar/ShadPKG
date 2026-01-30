# PS4 Memory Emulation Layer (PS4MEL)

Complete runtime emulation layer for decompiled PS4 games.

## Components

| File | Description |
|------|-------------|
| `ps4_memory.h/cpp` | 16MB global memory + address translation |
| `ps4_tls.h/cpp` | Thread Local Storage (Windows/POSIX) |
| `ps4_kernel.h/cpp` | 30+ sceKernel* syscalls |
| `ps4_pthread.h/cpp` | Threading primitives |
| `ps4_graphics.h/cpp` | VideoOut/GnmDriver stubs |
| `runtime.h/cpp` | Initialization + additional stubs |

## Architecture

```
┌────────────────────────────────────────────────────┐
│              Decompiled Game Code                  │
├────────────────────────────────────────────────────┤
│  PS4_GLOBAL_*  │ sceKernel* │ scePthread*         │
├────────────────┼────────────┼─────────────────────┤
│  ps4_memory    │ ps4_kernel │ ps4_pthread         │
│  (16MB region) │ (syscalls) │ (threading)         │
└────────────────────────────────────────────────────┘
```

## Usage

1. Include headers in your decompiled code
2. Call `runtime_init()` at program start
3. Call `runtime_shutdown()` before exit

## API Reference

### Memory (`ps4_memory.h`)
- `PS4_GLOBAL_I64(addr)` - Read 64-bit from PS4 address
- `PS4_GLOBAL_PTR(addr)` - Get pointer to PS4 address
- `TranslateAddress(addr)` - Convert PS4 → host address

### Kernel (`ps4_kernel.h`)
- `sceKernelOpen/Close/Read/Write` - File I/O
- `sceKernelMmap/Munmap/Mprotect` - Memory management
- `sceKernelGetProcessTime` - Timing
- `sceKernelUsleep/Nanosleep` - Sleep

### Threading (`ps4_pthread.h`)
- `scePthreadCreate/Join/Exit` - Thread lifecycle
- `scePthreadMutex*` - Mutex operations
- `scePthreadCond*` - Condition variables

### Graphics (`ps4_graphics.h`)
- `sceVideoOutOpen/Close` - Display initialization
- `sceGnmSubmitCommandBuffers` - GPU commands (stub)

## License

Part of ShadPKG project.