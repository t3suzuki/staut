
#ifndef __REAL_PTHREAD_H__
#define __REAL_PTHREAD_H__

#define _GNU_SOURCE
#include <pthread.h>
#include <dlfcn.h>

#define INLINE inline

#define dltrans(str) dlsym(dlopen("libpthread.so.0", RTLD_NOW), str)

INLINE int real_pthread_create(pthread_t *thread, const pthread_attr_t *attr,
			void *(*start_routine) (void *), void *arg) {
  int (*dlsym_pthread_create)(pthread_t *thread, const pthread_attr_t *attr,
			      void *(*start_routine) (void *), void *arg);
  //printf("%s %d\n", __func__, __LINE__);
  dlsym_pthread_create = (int (*)(pthread_t *, const pthread_attr_t *,
				  void *(*) (void *), void *))dltrans("pthread_create");
  int ret = dlsym_pthread_create(thread, attr, start_routine, arg);
  //printf("%s %d %d\n", __func__, __LINE__, ret);
  return ret;
}

INLINE int real_pthread_join(pthread_t thread, void **retval) {
  int (*dlsym_pthread_join)(pthread_t thread, void **retval);
  dlsym_pthread_join = (int (*)(pthread_t, void **))dltrans("pthread_join");
  int ret = dlsym_pthread_join(thread, retval);
  return ret;
}


INLINE int real_pthread_mutex_init(pthread_mutex_t * mutex,
			    const pthread_mutexattr_t * attr) {
  int (*dlsym_pthread_mutex_init)(pthread_mutex_t * mutex,
				 const pthread_mutexattr_t *  attr);
  dlsym_pthread_mutex_init = (int (*)(pthread_mutex_t *,
				      const pthread_mutexattr_t *))dltrans("pthread_mutex_init");
  int ret = dlsym_pthread_mutex_init(mutex, attr);
  //printf("%s %d %d\n", __func__, __LINE__, ret);
  return ret;
}

INLINE int real_pthread_mutex_destroy(pthread_mutex_t *mutex) {
  int (*dlsym_pthread_mutex_destroy)(pthread_mutex_t *mutex);
  dlsym_pthread_mutex_destroy = (int (*)(pthread_mutex_t *))dltrans("pthread_mutex_destroy");
  int ret = dlsym_pthread_mutex_destroy(mutex);
  return ret;
}

INLINE int real_pthread_mutex_lock(pthread_mutex_t *mutex) {
  int (*dlsym_pthread_mutex_lock)(pthread_mutex_t *mutex);
  dlsym_pthread_mutex_lock = (int (*)(pthread_mutex_t *))dltrans("pthread_mutex_lock");
  int ret = dlsym_pthread_mutex_lock(mutex);
  return ret;
}

INLINE int real_pthread_mutex_unlock(pthread_mutex_t *mutex) {
  int (*dlsym_pthread_mutex_unlock)(pthread_mutex_t *mutex);
  dlsym_pthread_mutex_unlock = (int (*)(pthread_mutex_t *))dltrans("pthread_mutex_unlock");
  int ret = dlsym_pthread_mutex_unlock(mutex);
  return ret;
}

INLINE int real_pthread_cond_init(pthread_cond_t *cond,
		      const pthread_condattr_t *attr) {
  int (*dlsym_pthread_cond_init)(pthread_cond_t *cond,
				 const pthread_condattr_t *attr);
  dlsym_pthread_cond_init = (int (*)(pthread_cond_t *,
				     const pthread_condattr_t *))dltrans("pthread_cond_init");
  int ret = dlsym_pthread_cond_init(cond, attr);
  //printf("%s %d %d\n", __func__, __LINE__, ret);
  return ret;
}

INLINE int real_pthread_cond_destroy(pthread_cond_t *cond) {
  int (*dlsym_pthread_cond_destroy)(pthread_cond_t *cond);
  dlsym_pthread_cond_destroy = (int (*)(pthread_cond_t *))dltrans("pthread_cond_destroy");
  int ret = dlsym_pthread_cond_destroy(cond);
  return ret;
}

INLINE int real_pthread_cond_signal(pthread_cond_t *cond) {
  int (*dlsym_pthread_cond_signal)(pthread_cond_t *cond);
  dlsym_pthread_cond_signal = (int (*)(pthread_cond_t *))dltrans("pthread_cond_signal");
  int ret = dlsym_pthread_cond_signal(cond);
  return ret;
}

INLINE int real_pthread_cond_broadcast(pthread_cond_t *cond) {
  int (*dlsym_pthread_cond_broadcast)(pthread_cond_t *cond);
  dlsym_pthread_cond_broadcast = (int (*)(pthread_cond_t *))dltrans("pthread_cond_broadcast");
  int ret = dlsym_pthread_cond_broadcast(cond);
  return ret;
}

INLINE int real_pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex) {
  int (*dlsym_pthread_cond_wait)(pthread_cond_t *cond, pthread_mutex_t *mutex);
  dlsym_pthread_cond_wait = (int (*)(pthread_cond_t *, pthread_mutex_t *))dltrans("pthread_cond_wait");
  int ret = dlsym_pthread_cond_wait(cond, mutex);
  return ret;
}

INLINE int real_pthread_cond_timedwait(pthread_cond_t *cond, pthread_mutex_t *mutex, const struct timespec *abstime) {
  int (*dlsym_pthread_cond_timedwait)(pthread_cond_t *cond, pthread_mutex_t *mutex, const struct timespec *abstime);
  dlsym_pthread_cond_timedwait = (int (*)(pthread_cond_t *, pthread_mutex_t *, const struct timespec *))dltrans("pthread_cond_timedwait");
  int ret = dlsym_pthread_cond_timedwait(cond, mutex, abstime);
  return ret;
}

INLINE int real_pthread_barrier_init(pthread_barrier_t *barrier, const pthread_barrierattr_t *attr, unsigned count) {
  int (*dlsym_pthread_barrier_init)(pthread_barrier_t *barrier, const pthread_barrierattr_t *attr, unsigned count);
  dlsym_pthread_barrier_init = (int (*)(pthread_barrier_t *, const pthread_barrierattr_t *, unsigned))dltrans("pthread_barrier_init");
  int ret = dlsym_pthread_barrier_init(barrier, attr, count);
  //printf("%s %d %d\n", __func__, __LINE__, ret);
  return ret;
}

INLINE int real_pthread_barrier_destroy(pthread_barrier_t *barrier) {
  int (*dlsym_pthread_barrier_destroy)(pthread_barrier_t *barrier);
  dlsym_pthread_barrier_destroy = (int (*)(pthread_barrier_t *))dltrans("pthread_barrier_destroy");
  int ret = dlsym_pthread_barrier_destroy(barrier);
  return ret;
}

INLINE int real_pthread_barrier_wait(pthread_barrier_t *barrier) {
  int (*dlsym_pthread_barrier_wait)(pthread_barrier_t *barrier);
  dlsym_pthread_barrier_wait = (int (*)(pthread_barrier_t *))dltrans("pthread_barrier_wait");
  int ret = dlsym_pthread_barrier_wait(barrier);
  return ret;
}

INLINE pthread_t real_pthread_self(void) {
  pthread_t (*dlsym_pthread_self)(void);
  dlsym_pthread_self = (pthread_t (*)(void))dltrans("pthread_self");
  return dlsym_pthread_self();
}

INLINE int real_pthread_setname_np(pthread_t pth, const char *name) {
  int (*dlsym_pthread_setname_np)(pthread_t pth, const char *name);
  dlsym_pthread_setname_np = (int (*)(pthread_t, const char *))dltrans("pthread_setname_np");
  int ret = dlsym_pthread_setname_np(pth, name);
  return ret;
}

INLINE int real_pthread_condattr_getclock(const pthread_condattr_t *attr, clockid_t *clock_id) {
  int (*dlsym_pthread_condattr_getclock)(const pthread_condattr_t *attr, clockid_t *clock_id);
  dlsym_pthread_condattr_getclock = (int (*)(const pthread_condattr_t *, clockid_t *))dltrans("pthread_condattr_getclock");
  int ret = dlsym_pthread_condattr_getclock(attr, clock_id);
  return ret;
}

#endif
