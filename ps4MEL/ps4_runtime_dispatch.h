#pragma once
// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║  PS4 Runtime Dispatch System                                               ║
// ║  Intercepts vtable/indirect calls and routes them to backend impls         ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

#include <cstdint>
#include <functional>
#include <unordered_map>
#include <string>
#include <iostream>
#include <mutex>

namespace PS4Runtime {

// ═══════════════════════════════════════════════════════════════════════════
// Semantic IDs for known PS4 functions
// ═══════════════════════════════════════════════════════════════════════════
enum class SemanticID : uint32_t {
    Unknown = 0,
    
    // GNM Graphics
    GNM_SubmitCommandBuffers,
    GNM_SubmitAndFlipCommandBuffers,
    GNM_DrawIndex,
    GNM_DrawIndexAuto,
    GNM_Dispatch,
    GNM_SetRenderTarget,
    GNM_SetViewport,
    GNM_SetScissor,
    GNM_ClearRenderTarget,
    GNM_WaitFlipDone,
    
    // VideoOut
    VideoOut_Open,
    VideoOut_Close,
    VideoOut_RegisterBuffers,
    VideoOut_SubmitFlip,
    VideoOut_GetFlipStatus,
    VideoOut_SetFlipRate,
    
    // Pad
    Pad_Open,
    Pad_Close,
    Pad_Read,
    Pad_ReadState,
    
    // Audio
    Audio_Open,
    Audio_Close,
    Audio_Output,
    
    // System
    Kernel_Usleep,
    Kernel_Nanosleep,
    
    MAX_SEMANTIC_ID
};

// ═══════════════════════════════════════════════════════════════════════════
// Function signature for dispatched calls
// Using void* for maximum flexibility - actual casting happens in impl
// ═══════════════════════════════════════════════════════════════════════════
using DispatchFn = std::function<int64_t(void* obj, uint64_t* args, int argCount)>;

// ═══════════════════════════════════════════════════════════════════════════
// Runtime Registry - maps function pointers to semantic implementations
// ═══════════════════════════════════════════════════════════════════════════
class RuntimeRegistry {
public:
    static RuntimeRegistry& Instance() {
        static RuntimeRegistry instance;
        return instance;
    }
    
    // Register a known function pointer -> semantic mapping
    void RegisterFunction(void* fnPtr, SemanticID id, const std::string& name) {
        std::lock_guard<std::mutex> lock(mutex_);
        fnToSemantic_[fnPtr] = id;
        semanticNames_[id] = name;
    }
    
    // Register a backend implementation for a semantic ID
    void RegisterBackend(SemanticID id, DispatchFn impl) {
        std::lock_guard<std::mutex> lock(mutex_);
        backends_[id] = impl;
    }
    
    // Resolve a function pointer to its semantic ID
    SemanticID Resolve(void* fnPtr) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = fnToSemantic_.find(fnPtr);
        if (it != fnToSemantic_.end()) {
            return it->second;
        }
        return SemanticID::Unknown;
    }
    
    // Get backend implementation for a semantic ID
    DispatchFn GetBackend(SemanticID id) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = backends_.find(id);
        if (it != backends_.end()) {
            return it->second;
        }
        return nullptr;
    }
    
    // Get name for logging
    const std::string& GetName(SemanticID id) {
        static std::string unknown = "Unknown";
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = semanticNames_.find(id);
        if (it != semanticNames_.end()) {
            return it->second;
        }
        return unknown;
    }
    
    // Log unknown function pointer for later analysis
    void LogUnknown(void* obj, uint64_t offset, void* fnPtr) {
        std::lock_guard<std::mutex> lock(mutex_);
        unknownCalls_[fnPtr]++;
        
        // Log first occurrence and every 1000th call
        if (unknownCalls_[fnPtr] == 1 || unknownCalls_[fnPtr] % 1000 == 0) {
            std::cout << "[DISPATCH] Unknown vtable call: obj=" << obj 
                      << " offset=0x" << std::hex << offset 
                      << " fn=" << fnPtr 
                      << " count=" << std::dec << unknownCalls_[fnPtr] 
                      << std::endl;
        }
    }
    
    // Get statistics
    void PrintStats() {
        std::lock_guard<std::mutex> lock(mutex_);
        std::cout << "\n[DISPATCH] === Runtime Dispatch Statistics ===" << std::endl;
        std::cout << "[DISPATCH] Known functions: " << fnToSemantic_.size() << std::endl;
        std::cout << "[DISPATCH] Unknown unique calls: " << unknownCalls_.size() << std::endl;
        
        std::cout << "[DISPATCH] Top unknown calls:" << std::endl;
        // Sort by count and show top 10
        std::vector<std::pair<void*, uint64_t>> sorted(unknownCalls_.begin(), unknownCalls_.end());
        std::sort(sorted.begin(), sorted.end(), 
                  [](const auto& a, const auto& b) { return a.second > b.second; });
        
        int shown = 0;
        for (const auto& [ptr, count] : sorted) {
            if (shown++ >= 10) break;
            std::cout << "  fn=" << ptr << " count=" << count << std::endl;
        }
    }

private:
    RuntimeRegistry() = default;
    
    std::mutex mutex_;
    std::unordered_map<void*, SemanticID> fnToSemantic_;
    std::unordered_map<SemanticID, std::string> semanticNames_;
    std::unordered_map<SemanticID, DispatchFn> backends_;
    std::unordered_map<void*, uint64_t> unknownCalls_;
};

// ═══════════════════════════════════════════════════════════════════════════
// Main dispatch function - called from decompiled code
// ═══════════════════════════════════════════════════════════════════════════

// Dispatch for vtable call: call [obj + offset]
inline int64_t ps4_vtable_dispatch(void* obj, uint64_t offset, 
                                    uint64_t a1 = 0, uint64_t a2 = 0, 
                                    uint64_t a3 = 0, uint64_t a4 = 0,
                                    uint64_t a5 = 0, uint64_t a6 = 0) {
    if (!obj) {
        std::cerr << "[DISPATCH] NULL object in vtable call, offset=0x" 
                  << std::hex << offset << std::endl;
        return 0;
    }
    
    // Read function pointer from vtable
    void* fnPtr = *reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(obj) + offset);
    
    auto& registry = RuntimeRegistry::Instance();
    SemanticID id = registry.Resolve(fnPtr);
    
    if (id != SemanticID::Unknown) {
        auto backend = registry.GetBackend(id);
        if (backend) {
            uint64_t args[] = {a1, a2, a3, a4, a5, a6};
            return backend(obj, args, 6);
        }
    }
    
    // Unknown function - log it
    registry.LogUnknown(obj, offset, fnPtr);
    return 0;
}

// Dispatch for indirect register call: call reg
inline int64_t ps4_indirect_dispatch(void* fnPtr,
                                      uint64_t a1 = 0, uint64_t a2 = 0,
                                      uint64_t a3 = 0, uint64_t a4 = 0,
                                      uint64_t a5 = 0, uint64_t a6 = 0) {
    if (!fnPtr) {
        std::cerr << "[DISPATCH] NULL function pointer in indirect call" << std::endl;
        return 0;
    }
    
    auto& registry = RuntimeRegistry::Instance();
    SemanticID id = registry.Resolve(fnPtr);
    
    if (id != SemanticID::Unknown) {
        auto backend = registry.GetBackend(id);
        if (backend) {
            uint64_t args[] = {a1, a2, a3, a4, a5, a6};
            return backend(nullptr, args, 6);
        }
    }
    
    // Unknown function - log it
    registry.LogUnknown(nullptr, 0, fnPtr);
    return 0;
}

// ═══════════════════════════════════════════════════════════════════════════
// Initialize the runtime dispatch system with known PS4 functions
// ═══════════════════════════════════════════════════════════════════════════
void InitializeDispatch();

} // namespace PS4Runtime

// C-compatible wrappers for use in decompiled code
extern "C" {
    int64_t ps4_vtable_dispatch_c(void* obj, uint64_t offset,
                                   uint64_t a1, uint64_t a2, uint64_t a3,
                                   uint64_t a4, uint64_t a5, uint64_t a6);
    
    int64_t ps4_indirect_dispatch_c(void* fnPtr,
                                     uint64_t a1, uint64_t a2, uint64_t a3,
                                     uint64_t a4, uint64_t a5, uint64_t a6);
}
