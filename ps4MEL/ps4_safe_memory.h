/*
 * PS4 Safe Memory Access Layer
 * Protects against invalid memory accesses in decompiled code
 */

#ifndef PS4_SAFE_MEMORY_H
#define PS4_SAFE_MEMORY_H

#include <cstdint>
#include <cstring>

// Safe memory read - returns 0 for invalid addresses
template<typename T>
inline T safe_read(uintptr_t addr) {
    extern uint8_t g_ps4_memory[];
    extern const size_t G_PS4_MEMORY_SIZE;
    
    // Check if address is within g_ps4_memory bounds
    if (addr < G_PS4_MEMORY_SIZE) {
        return *reinterpret_cast<T*>(&g_ps4_memory[addr]);
    }
    
    // Check if it's a pointer into g_ps4_memory
    uintptr_t memBase = reinterpret_cast<uintptr_t>(g_ps4_memory);
    if (addr >= memBase && addr < memBase + G_PS4_MEMORY_SIZE) {
        return *reinterpret_cast<T*>(addr);
    }
    
    // Invalid address - return 0
    return T(0);
}

// Safe memory write - ignores writes to invalid addresses
template<typename T>
inline void safe_write(uintptr_t addr, T value) {
    extern uint8_t g_ps4_memory[];
    extern const size_t G_PS4_MEMORY_SIZE;
    
    if (addr < G_PS4_MEMORY_SIZE) {
        *reinterpret_cast<T*>(&g_ps4_memory[addr]) = value;
        return;
    }
    
    uintptr_t memBase = reinterpret_cast<uintptr_t>(g_ps4_memory);
    if (addr >= memBase && addr < memBase + G_PS4_MEMORY_SIZE) {
        *reinterpret_cast<T*>(addr) = value;
    }
    // Otherwise silently ignore
}

// Safe pointer dereference
template<typename T>
inline T safe_deref(T* ptr) {
    if (ptr == nullptr) return T(0);
    
    extern uint8_t g_ps4_memory[];
    extern const size_t G_PS4_MEMORY_SIZE;
    
    uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);
    uintptr_t memBase = reinterpret_cast<uintptr_t>(g_ps4_memory);
    
    // Check if pointer is within valid memory
    if (addr >= memBase && addr + sizeof(T) <= memBase + G_PS4_MEMORY_SIZE) {
        return *ptr;
    }
    
    // Check if it's a small offset (likely g_ps4_memory index)
    if (addr < G_PS4_MEMORY_SIZE) {
        return *reinterpret_cast<T*>(&g_ps4_memory[addr]);
    }
    
    return T(0);
}

// Safe pointer dereference for decompiled code - used by safe_deref_ptr<T>(ptr)
template<typename T>
inline T safe_deref_ptr(T* ptr) {
    if (ptr == nullptr) return T(0);
    
    extern uint8_t g_ps4_memory[];
    
    uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);
    uintptr_t memBase = reinterpret_cast<uintptr_t>(g_ps4_memory);
    constexpr size_t memSize = 0x1000000; // 16MB
    
    // Check if pointer is within g_ps4_memory bounds
    if (addr >= memBase && addr + sizeof(T) <= memBase + memSize) {
        return *ptr;
    }
    
    // Check if it's a small offset (index into g_ps4_memory)
    if (addr < memSize) {
        return *reinterpret_cast<T*>(&g_ps4_memory[addr]);
    }
    
    // Invalid pointer - return 0 to avoid crash
    return T(0);
}

#endif // PS4_SAFE_MEMORY_H
