#ifndef __ULT_H__
#define __ULT_H__

#if USE_PTHPTH

#define _GNU_SOURCE
#include <unistd.h>
#include <sys/types.h>
#include <pthread.h>
typedef pthread_mutex_t ult_mutex;
#define ult_mutex_create(x) pthread_mutex_init(x, NULL)
#define ult_mutex_lock(x)   pthread_mutex_lock(x)
#define ult_mutex_unlock(x) pthread_mutex_unlock(x)
#define ult_yield() sched_yield()
#define is_ult() (1)
#define ult_id() (gettid())
#define ult_is_cswitchable() (1)

static inline int ult_core_id()
{
  return (ult_id() % N_CORE);
}

#else

typedef ABT_mutex ult_mutex;
#define ult_mutex_create(x) ABT_mutex_create(x)
#define ult_mutex_lock(x)   ABT_mutex_lock(*x)
#define ult_mutex_unlock(x) ABT_mutex_unlock(*x)
#define ult_yield() ABT_thread_yield()
#define ult_is_cswitchable() !ABT_thread_is_sched()

/*
extern int mylib_initialized;
static inline void ult_yield()
{
  if (mylib_initialized)
    ABT_thread_yield();
}
*/

static inline int is_ult()
{
  uint64_t abt_id;
  int ret = ABT_thread_self_id(&abt_id);
  return ((ret == ABT_SUCCESS) && (abt_id >= 0));
}

static inline uint64_t ult_id()
{
  uint64_t abt_id;
  int ret = ABT_thread_self_id(&abt_id);
  return abt_id;
}

static inline int ult_core_id()
{
  int rank;
  ABT_xstream_self_rank(&rank);
  return rank;
}

#endif

#endif // __ULT_H__
