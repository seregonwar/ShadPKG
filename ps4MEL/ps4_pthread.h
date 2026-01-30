/*
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║                     PS4 PTHREAD STUBS - HEADER                            ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║  Emulates scePthread* threading primitives for PS4 compatibility.         ║
 * ╚═══════════════════════════════════════════════════════════════════════════╝
 */

#pragma once

#include <cstdint>

/*
 * ┌─────────────────────────────────────────────────────────────────┐
 * │                    THREAD TYPES                                 │
 * └─────────────────────────────────────────────────────────────────┘
 */

typedef void *ScePthread;
typedef void *ScePthreadAttr;
typedef void *ScePthreadMutex;
typedef void *ScePthreadMutexattr;
typedef void *ScePthreadCond;
typedef void *ScePthreadCondattr;
typedef void *ScePthreadRwlock;
typedef void *ScePthreadRwlockattr;

/*
 * ┌─────────────────────────────────────────────────────────────────┐
 * │                    THREAD CONSTANTS                             │
 * └─────────────────────────────────────────────────────────────────┘
 */

#define SCE_PTHREAD_MUTEX_INITIALIZER nullptr
#define SCE_PTHREAD_COND_INITIALIZER nullptr
#define SCE_PTHREAD_RWLOCK_INITIALIZER nullptr

// Mutex types
#define SCE_PTHREAD_MUTEX_ERRORCHECK 1
#define SCE_PTHREAD_MUTEX_RECURSIVE 2
#define SCE_PTHREAD_MUTEX_NORMAL 3
#define SCE_PTHREAD_MUTEX_ADAPTIVE_NP 4

// Thread priorities
#define SCE_KERNEL_PRIO_FIFO_DEFAULT 700
#define SCE_KERNEL_PRIO_FIFO_HIGHEST 256
#define SCE_KERNEL_PRIO_FIFO_LOWEST 767

/*
 * ┌─────────────────────────────────────────────────────────────────┐
 * │                    THREAD FUNCTIONS                             │
 * └─────────────────────────────────────────────────────────────────┘
 */

extern "C" {

// ═══════════════════════════════════════════════════════════════════
// THREAD LIFECYCLE
// ═══════════════════════════════════════════════════════════════════
int32_t scePthreadCreate(ScePthread *thread, const ScePthreadAttr *attr,
                         void *(*entry)(void *), void *arg, const char *name);
int32_t scePthreadJoin(ScePthread thread, void **retval);
void scePthreadExit(void *retval);
ScePthread scePthreadSelf();
int32_t scePthreadCancel(ScePthread thread);
int32_t scePthreadDetach(ScePthread thread);

// ═══════════════════════════════════════════════════════════════════
// THREAD ATTRIBUTES
// ═══════════════════════════════════════════════════════════════════
int32_t scePthreadAttrInit(ScePthreadAttr *attr);
int32_t scePthreadAttrDestroy(ScePthreadAttr *attr);
int32_t scePthreadAttrSetdetachstate(ScePthreadAttr *attr, int32_t state);
int32_t scePthreadAttrSetstacksize(ScePthreadAttr *attr, uint64_t size);
int32_t scePthreadAttrSetschedparam(ScePthreadAttr *attr, const void *param);

// ═══════════════════════════════════════════════════════════════════
// MUTEX
// ═══════════════════════════════════════════════════════════════════
int32_t scePthreadMutexInit(ScePthreadMutex *mutex,
                            const ScePthreadMutexattr *attr, const char *name);
int32_t scePthreadMutexDestroy(ScePthreadMutex *mutex);
int32_t scePthreadMutexLock(ScePthreadMutex *mutex);
int32_t scePthreadMutexTrylock(ScePthreadMutex *mutex);
int32_t scePthreadMutexUnlock(ScePthreadMutex *mutex);
int32_t scePthreadMutexattrInit(ScePthreadMutexattr *attr);
int32_t scePthreadMutexattrDestroy(ScePthreadMutexattr *attr);
int32_t scePthreadMutexattrSettype(ScePthreadMutexattr *attr, int32_t type);

// ═══════════════════════════════════════════════════════════════════
// CONDITION VARIABLE
// ═══════════════════════════════════════════════════════════════════
int32_t scePthreadCondInit(ScePthreadCond *cond, const ScePthreadCondattr *attr,
                           const char *name);
int32_t scePthreadCondDestroy(ScePthreadCond *cond);
int32_t scePthreadCondSignal(ScePthreadCond *cond);
int32_t scePthreadCondBroadcast(ScePthreadCond *cond);
int32_t scePthreadCondWait(ScePthreadCond *cond, ScePthreadMutex *mutex);
int32_t scePthreadCondTimedwait(ScePthreadCond *cond, ScePthreadMutex *mutex,
                                uint32_t usec);

// ═══════════════════════════════════════════════════════════════════
// READ-WRITE LOCK
// ═══════════════════════════════════════════════════════════════════
int32_t scePthreadRwlockInit(ScePthreadRwlock *rwlock,
                             const ScePthreadRwlockattr *attr,
                             const char *name);
int32_t scePthreadRwlockDestroy(ScePthreadRwlock *rwlock);
int32_t scePthreadRwlockRdlock(ScePthreadRwlock *rwlock);
int32_t scePthreadRwlockWrlock(ScePthreadRwlock *rwlock);
int32_t scePthreadRwlockUnlock(ScePthreadRwlock *rwlock);

// ═══════════════════════════════════════════════════════════════════
// THREAD-SPECIFIC DATA
// ═══════════════════════════════════════════════════════════════════
int32_t scePthreadKeyCreate(uint32_t *key, void (*destructor)(void *));
int32_t scePthreadKeyDelete(uint32_t key);
void *scePthreadGetspecific(uint32_t key);
int32_t scePthreadSetspecific(uint32_t key, const void *value);

// ═══════════════════════════════════════════════════════════════════
// ONCE
// ═══════════════════════════════════════════════════════════════════
int32_t scePthreadOnce(void *once_control, void (*init_routine)());

// ═══════════════════════════════════════════════════════════════════
// YIELD
// ═══════════════════════════════════════════════════════════════════
int32_t scePthreadYield();

} // extern "C"
