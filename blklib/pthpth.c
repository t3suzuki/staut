#define _GNU_SOURCE
#include <sched.h>

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include "ult.h"
#include "real_pthread.h"


typedef void *(*func_t)(void *);

struct pth_wrap_t {
  func_t func;
  void *arg;
};

static
void pth_wrapper(void *arg)
{
  struct pth_wrap_t *pth_wrap = (struct pth_wrap_t *)arg;
  {
    cpu_set_t cpu_set;
    CPU_ZERO(&cpu_set);
    CPU_SET(ult_core_id(), &cpu_set);
    sched_setaffinity(0, sizeof(cpu_set_t), &cpu_set);
  }
  //printf("%s %p %p\n", __func__, pth_wrap->func, pth_wrap->arg);
  pth_wrap->func(pth_wrap->arg);
  free(pth_wrap);
}

int pthread_create(pthread_t *pth, const pthread_attr_t *attr,
		   void *(*start_routine) (void *), void *arg) {
  struct pth_wrap_t *pth_wrap = malloc(sizeof(struct pth_wrap_t));
  pth_wrap->func = start_routine;
  pth_wrap->arg = arg;
  //printf("%s %p %p\n", __func__, start_routine, arg);
  int ret = real_pthread_create(pth, attr, (void *(*)(void *))pth_wrapper, pth_wrap);
  return 0;
}

void yield_inifinite_loop(void)
{
}

void __zpoline_init(void);

int pthpth_initialized = 0;

__attribute__((constructor(0xffff))) static void
pthpth_init()
{
  if (!pthpth_initialized) {
    printf("%s %d\n", __func__, __LINE__);
    __zpoline_init();
    pthpth_initialized = 1;
  }
}
