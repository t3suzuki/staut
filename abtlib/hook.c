#include "ult.h"
#include <time.h>
#include <assert.h>

extern void (*debug_print)(int, int, int);

typedef long (*syscall_fn_t)(long, long, long, long, long, long, long);
static syscall_fn_t next_sys_call = 0;

static inline int
hook_clock_nanosleep(clockid_t which_clock, int flags,
		     const struct timespec *req,
		     struct timespec *rem)
{
  assert(which_clock == CLOCK_REALTIME);
  assert(flags == 0); // implemented relative time only.
  //printf("%s clock=%d flags=%d tv_sec %d tv_nsec %d\n", __func__, which_clock, flags, req->tv_sec, req->tv_nsec);
  
  struct timespec alarm;
  clock_gettime(CLOCK_REALTIME_COARSE, &alarm);
  alarm.tv_sec += req->tv_sec;
  alarm.tv_nsec += req->tv_nsec;
  while (1) {
    struct timespec now;
    clock_gettime(CLOCK_REALTIME_COARSE, &now);
    double diff_nsec = (now.tv_sec - alarm.tv_sec) * 1e9 + (now.tv_nsec - alarm.tv_nsec);
    if (diff_nsec > 0)
      return 0;
    ult_yield();
  }
  return 0;
}

long hook_function(long a1, long a2, long a3,
		   long a4, long a5, long a6,
		   long a7)
{
  if (debug_print) {
    debug_print(1, a1, 9999);
  }
  if (is_ult()) {
    switch (a1) {
    case 230: // nanosleep
      return hook_clock_nanosleep(a2, a3, (struct timespec *)a4, (struct timespec *)a5);
    }
  }
  return next_sys_call(a1, a2, a3, a4, a5, a6, a7);
}

int __hook_init(long placeholder __attribute__((unused)),
		void *sys_call_hook_ptr)
{
  load_debug();
  
  next_sys_call = *((syscall_fn_t *) sys_call_hook_ptr);
  *((syscall_fn_t *) sys_call_hook_ptr) = hook_function;

  return 0;
}

__attribute__((destructor(0xffff))) static void
hook_deinit()
{
}

