/* pool.c: rows of a big pixel job shared out over the machine's cores. The job's function gets bands of rows; the
   calling thread takes bands too and returns when all are done. Workers sleep between jobs. The result does not depend
   on how many threads there are. RK_THREADS sets the count (1: everything on the calling thread). */
#include "rk_internal.h"

#if defined(_WIN32)
#include <windows.h>
typedef CRITICAL_SECTION pool_lock;
typedef CONDITION_VARIABLE pool_cond;
#define P_LOCK(l) EnterCriticalSection(l)
#define P_UNLOCK(l) LeaveCriticalSection(l)
#define P_WAIT(c, l) SleepConditionVariableCS(c, l, INFINITE)
#define P_WAKE_ALL(c) WakeAllConditionVariable(c)
#else
#include <pthread.h>
#include <unistd.h>
typedef pthread_mutex_t pool_lock;
typedef pthread_cond_t pool_cond;
#define P_LOCK(l) pthread_mutex_lock(l)
#define P_UNLOCK(l) pthread_mutex_unlock(l)
#define P_WAIT(c, l) pthread_cond_wait(c, l)
#define P_WAKE_ALL(c) pthread_cond_broadcast(c)
#endif

#define BAND 24                                            /* rows a thread takes at a time */

static bool started;
static struct {
  int threads;                                             /* workers, besides the calling thread */
  pool_lock lock;
  pool_cond work, done;
  rk_rows_fn fn; void *ctx;
  int next, end, busy;                                     /* the next band's first row, the job's end, workers on it */
  unsigned job;                                            /* counts jobs, so a worker knows a new one is there */
} P;

/* Takes bands until none are left; called with the lock held, returns with it held. */
static void take_bands(void) {
  while (P.next < P.end) {
    int y0 = P.next, y1 = RK_MIN(P.end, y0 + BAND);
    P.next = y1;
    rk_rows_fn fn = P.fn; void *ctx = P.ctx;
    P_UNLOCK(&P.lock);
    fn(ctx, y0, y1);
    P_LOCK(&P.lock);
  }
}

#if defined(_WIN32)
static DWORD WINAPI worker(LPVOID arg) {
#else
static void *worker(void *arg) {
#endif
  (void)arg;
  unsigned seen = 0;
  P_LOCK(&P.lock);
  for (;;) {
    while (P.job == seen) P_WAIT(&P.work, &P.lock);
    seen = P.job;
    P.busy++;
    take_bands();
    if (--P.busy == 0) P_WAKE_ALL(&P.done);
  }
#if defined(_WIN32)
  return 0;
#else
  return NULL;
#endif
}

static void start(void) {
  const char *env = getenv("RK_THREADS");
  int n = env && *env ? atoi(env) : 0;
  if (n <= 0) {
#if defined(_WIN32)
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    n = (int)si.dwNumberOfProcessors;
#elif defined(_SC_NPROCESSORS_ONLN)
    n = (int)sysconf(_SC_NPROCESSORS_ONLN);
#else
    n = 1;
#endif
  }
  n = RK_CLAMP(n, 1, 8) - 1;                               /* the calling thread is one of them */
#if defined(_WIN32)
  InitializeCriticalSection(&P.lock);
  InitializeConditionVariable(&P.work);
  InitializeConditionVariable(&P.done);
#else
  pthread_mutex_init(&P.lock, NULL);
  pthread_cond_init(&P.work, NULL);
  pthread_cond_init(&P.done, NULL);
#endif
  P.threads = 0;
  started = true;
  for (int i = 0; i < n; i++) {
#if defined(_WIN32)
    HANDLE h = CreateThread(NULL, 0, worker, NULL, 0, NULL);
    if (!h) break;
    CloseHandle(h);
#else
    pthread_t t;
    if (pthread_create(&t, NULL, worker, NULL)) break;
    pthread_detach(t);
#endif
    P.threads++;
  }
}

void rk__parallel_rows(rk_rows_fn fn, void *ctx, int y0, int y1, long pixels) {
  if (y1 <= y0) return;
  if (!started) start();
  if (!P.threads || pixels < 200000 || y1 - y0 < 2 * BAND) { fn(ctx, y0, y1); return; }   /* small: not worth waking anyone */
  P_LOCK(&P.lock);
  P.fn = fn; P.ctx = ctx; P.next = y0; P.end = y1;
  P.job++;
  P_WAKE_ALL(&P.work);
  take_bands();
  while (P.busy > 0) P_WAIT(&P.done, &P.lock);
  P_UNLOCK(&P.lock);
}

/* Wall time in ms, for measuring frames (CPU time would count every thread) */
double rk__wall_ms(void) {
#if defined(_WIN32)
  static LARGE_INTEGER f;
  LARGE_INTEGER t;
  if (!f.QuadPart) QueryPerformanceFrequency(&f);
  QueryPerformanceCounter(&t);
  return (double)t.QuadPart * 1000.0 / (double)f.QuadPart;
#elif defined(CLOCK_MONOTONIC)
  struct timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);
  return t.tv_sec * 1000.0 + t.tv_nsec / 1e6;
#else
  return (double)clock() * 1000.0 / CLOCKS_PER_SEC;
#endif
}
