/*
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║                   PS4 PTHREAD STUBS - IMPLEMENTATION                      ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║  Real pthread wrappers for PS4 threading primitives.                      ║
 * ╚═══════════════════════════════════════════════════════════════════════════╝
 */

#include "../include/ps4_pthread.h"
#include "../include/ps4_kernel.h"
#include "../include/ps4_tls.h"
#include <condition_variable>
#include <cstring>
#include <iostream>
#include <map>
#include <mutex>
#include <shared_mutex>
#include <thread>

/*
 * ┌─────────────────────────────────────────────────────────────────┐
 * │                    INTERNAL STRUCTURES                          │
 * └─────────────────────────────────────────────────────────────────┘
 */

struct PS4Thread {
  std::thread native;
  void *(*entry)(void *);
  void *arg;
  void *retval;
  char name[32];
  bool detached;
  PS4Emu::Tcb *tcb;
};

struct PS4Mutex {
  std::recursive_mutex native;
  char name[32];
  int32_t type;
};

struct PS4Cond {
  std::condition_variable_any native;
  char name[32];
};

struct PS4Rwlock {
  std::shared_mutex native;
  char name[32];
};

/*
 * ┌─────────────────────────────────────────────────────────────────┐
 * │                    THREAD REGISTRY                              │
 * └─────────────────────────────────────────────────────────────────┘
 */

static std::map<ScePthread, PS4Thread *> g_threads;
static std::mutex g_threads_lock;
static thread_local PS4Thread *g_current_thread = nullptr;

// Thread-specific data
static std::map<uint32_t, std::map<std::thread::id, void *>> g_tsd;
static std::map<uint32_t, void (*)(void *)> g_tsd_destructors;
static std::mutex g_tsd_lock;
static uint32_t g_next_key = 1;

/*
 * ┌─────────────────────────────────────────────────────────────────┐
 * │                    THREAD WRAPPER                               │
 * └─────────────────────────────────────────────────────────────────┘
 */

static void ThreadWrapper(PS4Thread *thrd) {
  // Set up TLS for this thread
  thrd->tcb = PS4Emu::CreateTcb();
  PS4Emu::SetTcbBase(thrd->tcb);
  g_current_thread = thrd;

  std::cout << "[PS4Thread] Thread '" << thrd->name << "' started" << std::endl;

  // Call the actual entry point
  thrd->retval = thrd->entry(thrd->arg);

  std::cout << "[PS4Thread] Thread '" << thrd->name << "' exited" << std::endl;

  // Cleanup TLS
  PS4Emu::DestroyTcb(thrd->tcb);
}

extern "C" {

/*
 * ┌─────────────────────────────────────────────────────────────────┐
 * │                    THREAD LIFECYCLE                             │
 * └─────────────────────────────────────────────────────────────────┘
 */

int32_t scePthreadCreate(ScePthread *thread, const ScePthreadAttr *attr,
                         void *(*entry)(void *), void *arg, const char *name) {
  std::cout << "[PS4Thread] scePthreadCreate(\"" << (name ? name : "unnamed")
            << "\")" << std::endl;

  PS4Thread *thrd = new PS4Thread();
  thrd->entry = entry;
  thrd->arg = arg;
  thrd->retval = nullptr;
  thrd->detached = false;
  thrd->tcb = nullptr;
  strncpy(thrd->name, name ? name : "thread", sizeof(thrd->name) - 1);

  // Start the thread
  thrd->native = std::thread(ThreadWrapper, thrd);

  {
    std::lock_guard<std::mutex> lock(g_threads_lock);
    g_threads[thrd] = thrd;
  }

  *thread = thrd;
  return ORBIS_OK;
}

int32_t scePthreadJoin(ScePthread thread, void **retval) {
  PS4Thread *thrd = static_cast<PS4Thread *>(thread);
  if (!thrd)
    return ORBIS_KERNEL_ERROR_EINVAL;

  std::cout << "[PS4Thread] scePthreadJoin(\"" << thrd->name << "\")"
            << std::endl;

  if (thrd->native.joinable()) {
    thrd->native.join();
  }

  if (retval) {
    *retval = thrd->retval;
  }

  {
    std::lock_guard<std::mutex> lock(g_threads_lock);
    g_threads.erase(thrd);
  }

  delete thrd;
  return ORBIS_OK;
}

void scePthreadExit(void *retval) {
  std::cout << "[PS4Thread] scePthreadExit" << std::endl;
  if (g_current_thread) {
    g_current_thread->retval = retval;
  }
  // Note: Can't actually exit std::thread from within
}

ScePthread scePthreadSelf() { return g_current_thread; }

int32_t scePthreadCancel(ScePthread thread) {
  // std::thread doesn't support cancellation
  std::cout << "[PS4Thread] scePthreadCancel (not implemented)" << std::endl;
  return ORBIS_OK;
}

int32_t scePthreadDetach(ScePthread thread) {
  PS4Thread *thrd = static_cast<PS4Thread *>(thread);
  if (!thrd)
    return ORBIS_KERNEL_ERROR_EINVAL;

  if (thrd->native.joinable()) {
    thrd->native.detach();
  }
  thrd->detached = true;
  return ORBIS_OK;
}

/*
 * ┌─────────────────────────────────────────────────────────────────┐
 * │                    THREAD ATTRIBUTES                            │
 * └─────────────────────────────────────────────────────────────────┘
 */

int32_t scePthreadAttrInit(ScePthreadAttr *attr) {
  *attr = new uint8_t[64]();
  return ORBIS_OK;
}

int32_t scePthreadAttrDestroy(ScePthreadAttr *attr) {
  delete[] static_cast<uint8_t *>(*attr);
  *attr = nullptr;
  return ORBIS_OK;
}

int32_t scePthreadAttrSetdetachstate(ScePthreadAttr *attr, int32_t state) {
  return ORBIS_OK;
}

int32_t scePthreadAttrSetstacksize(ScePthreadAttr *attr, uint64_t size) {
  return ORBIS_OK;
}

int32_t scePthreadAttrSetschedparam(ScePthreadAttr *attr, const void *param) {
  return ORBIS_OK;
}

/*
 * ┌─────────────────────────────────────────────────────────────────┐
 * │                    MUTEX                                        │
 * └─────────────────────────────────────────────────────────────────┘
 */

int32_t scePthreadMutexInit(ScePthreadMutex *mutex,
                            const ScePthreadMutexattr *attr, const char *name) {
  PS4Mutex *mtx = new PS4Mutex();
  mtx->type = SCE_PTHREAD_MUTEX_NORMAL;
  strncpy(mtx->name, name ? name : "mutex", sizeof(mtx->name) - 1);
  *mutex = mtx;
  return ORBIS_OK;
}

int32_t scePthreadMutexDestroy(ScePthreadMutex *mutex) {
  PS4Mutex *mtx = static_cast<PS4Mutex *>(*mutex);
  if (mtx) {
    delete mtx;
    *mutex = nullptr;
  }
  return ORBIS_OK;
}

int32_t scePthreadMutexLock(ScePthreadMutex *mutex) {
  PS4Mutex *mtx = static_cast<PS4Mutex *>(*mutex);
  if (!mtx)
    return ORBIS_KERNEL_ERROR_EINVAL;
  mtx->native.lock();
  return ORBIS_OK;
}

int32_t scePthreadMutexTrylock(ScePthreadMutex *mutex) {
  PS4Mutex *mtx = static_cast<PS4Mutex *>(*mutex);
  if (!mtx)
    return ORBIS_KERNEL_ERROR_EINVAL;
  return mtx->native.try_lock() ? ORBIS_OK : ORBIS_KERNEL_ERROR_EBUSY;
}

int32_t scePthreadMutexUnlock(ScePthreadMutex *mutex) {
  PS4Mutex *mtx = static_cast<PS4Mutex *>(*mutex);
  if (!mtx)
    return ORBIS_KERNEL_ERROR_EINVAL;
  mtx->native.unlock();
  return ORBIS_OK;
}

int32_t scePthreadMutexattrInit(ScePthreadMutexattr *attr) {
  *attr = new int32_t(SCE_PTHREAD_MUTEX_NORMAL);
  return ORBIS_OK;
}

int32_t scePthreadMutexattrDestroy(ScePthreadMutexattr *attr) {
  delete static_cast<int32_t *>(*attr);
  *attr = nullptr;
  return ORBIS_OK;
}

int32_t scePthreadMutexattrSettype(ScePthreadMutexattr *attr, int32_t type) {
  *static_cast<int32_t *>(*attr) = type;
  return ORBIS_OK;
}

/*
 * ┌─────────────────────────────────────────────────────────────────┐
 * │                    CONDITION VARIABLE                           │
 * └─────────────────────────────────────────────────────────────────┘
 */

int32_t scePthreadCondInit(ScePthreadCond *cond, const ScePthreadCondattr *attr,
                           const char *name) {
  PS4Cond *cv = new PS4Cond();
  strncpy(cv->name, name ? name : "cond", sizeof(cv->name) - 1);
  *cond = cv;
  return ORBIS_OK;
}

int32_t scePthreadCondDestroy(ScePthreadCond *cond) {
  PS4Cond *cv = static_cast<PS4Cond *>(*cond);
  if (cv) {
    delete cv;
    *cond = nullptr;
  }
  return ORBIS_OK;
}

int32_t scePthreadCondSignal(ScePthreadCond *cond) {
  PS4Cond *cv = static_cast<PS4Cond *>(*cond);
  if (!cv)
    return ORBIS_KERNEL_ERROR_EINVAL;
  cv->native.notify_one();
  return ORBIS_OK;
}

int32_t scePthreadCondBroadcast(ScePthreadCond *cond) {
  PS4Cond *cv = static_cast<PS4Cond *>(*cond);
  if (!cv)
    return ORBIS_KERNEL_ERROR_EINVAL;
  cv->native.notify_all();
  return ORBIS_OK;
}

int32_t scePthreadCondWait(ScePthreadCond *cond, ScePthreadMutex *mutex) {
  PS4Cond *cv = static_cast<PS4Cond *>(*cond);
  PS4Mutex *mtx = static_cast<PS4Mutex *>(*mutex);
  if (!cv || !mtx)
    return ORBIS_KERNEL_ERROR_EINVAL;
  cv->native.wait(mtx->native);
  return ORBIS_OK;
}

int32_t scePthreadCondTimedwait(ScePthreadCond *cond, ScePthreadMutex *mutex,
                                uint32_t usec) {
  PS4Cond *cv = static_cast<PS4Cond *>(*cond);
  PS4Mutex *mtx = static_cast<PS4Mutex *>(*mutex);
  if (!cv || !mtx)
    return ORBIS_KERNEL_ERROR_EINVAL;

  auto status =
      cv->native.wait_for(mtx->native, std::chrono::microseconds(usec));
  return status == std::cv_status::timeout ? ORBIS_KERNEL_ERROR_ETIMEDOUT
                                           : ORBIS_OK;
}

/*
 * ┌─────────────────────────────────────────────────────────────────┐
 * │                    READ-WRITE LOCK                              │
 * └─────────────────────────────────────────────────────────────────┘
 */

int32_t scePthreadRwlockInit(ScePthreadRwlock *rwlock,
                             const ScePthreadRwlockattr *attr,
                             const char *name) {
  PS4Rwlock *rw = new PS4Rwlock();
  strncpy(rw->name, name ? name : "rwlock", sizeof(rw->name) - 1);
  *rwlock = rw;
  return ORBIS_OK;
}

int32_t scePthreadRwlockDestroy(ScePthreadRwlock *rwlock) {
  PS4Rwlock *rw = static_cast<PS4Rwlock *>(*rwlock);
  if (rw) {
    delete rw;
    *rwlock = nullptr;
  }
  return ORBIS_OK;
}

int32_t scePthreadRwlockRdlock(ScePthreadRwlock *rwlock) {
  PS4Rwlock *rw = static_cast<PS4Rwlock *>(*rwlock);
  if (!rw)
    return ORBIS_KERNEL_ERROR_EINVAL;
  rw->native.lock_shared();
  return ORBIS_OK;
}

int32_t scePthreadRwlockWrlock(ScePthreadRwlock *rwlock) {
  PS4Rwlock *rw = static_cast<PS4Rwlock *>(*rwlock);
  if (!rw)
    return ORBIS_KERNEL_ERROR_EINVAL;
  rw->native.lock();
  return ORBIS_OK;
}

int32_t scePthreadRwlockUnlock(ScePthreadRwlock *rwlock) {
  PS4Rwlock *rw = static_cast<PS4Rwlock *>(*rwlock);
  if (!rw)
    return ORBIS_KERNEL_ERROR_EINVAL;
  // Note: shared_mutex doesn't track read vs write lock
  // This is a simplification
  rw->native.unlock();
  return ORBIS_OK;
}

/*
 * ┌─────────────────────────────────────────────────────────────────┐
 * │                    THREAD-SPECIFIC DATA                         │
 * └─────────────────────────────────────────────────────────────────┘
 */

int32_t scePthreadKeyCreate(uint32_t *key, void (*destructor)(void *)) {
  std::lock_guard<std::mutex> lock(g_tsd_lock);
  uint32_t k = g_next_key++;
  g_tsd_destructors[k] = destructor;
  *key = k;
  return ORBIS_OK;
}

int32_t scePthreadKeyDelete(uint32_t key) {
  std::lock_guard<std::mutex> lock(g_tsd_lock);
  g_tsd.erase(key);
  g_tsd_destructors.erase(key);
  return ORBIS_OK;
}

void *scePthreadGetspecific(uint32_t key) {
  std::lock_guard<std::mutex> lock(g_tsd_lock);
  auto it = g_tsd.find(key);
  if (it == g_tsd.end())
    return nullptr;
  auto tid = std::this_thread::get_id();
  auto it2 = it->second.find(tid);
  return it2 != it->second.end() ? it2->second : nullptr;
}

int32_t scePthreadSetspecific(uint32_t key, const void *value) {
  std::lock_guard<std::mutex> lock(g_tsd_lock);
  g_tsd[key][std::this_thread::get_id()] = const_cast<void *>(value);
  return ORBIS_OK;
}

/*
 * ┌─────────────────────────────────────────────────────────────────┐
 * │                    ONCE / YIELD                                 │
 * └─────────────────────────────────────────────────────────────────┘
 */

int32_t scePthreadOnce(void *once_control, void (*init_routine)()) {
  static std::mutex once_mutex;
  static std::map<void *, bool> once_done;

  std::lock_guard<std::mutex> lock(once_mutex);
  if (!once_done[once_control]) {
    init_routine();
    once_done[once_control] = true;
  }
  return ORBIS_OK;
}

int32_t scePthreadYield() {
  std::this_thread::yield();
  return ORBIS_OK;
}

} // extern "C"
