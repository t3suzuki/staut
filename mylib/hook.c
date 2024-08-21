#define _GNU_SOURCE
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <immintrin.h>

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <assert.h>
#include <time.h>
#include <linux/aio_abi.h>
#include <linux/futex.h>
//#include <liburing.h>
#include <sys/syscall.h>

#include "iouringlib.h"
#include "real_pthread.h"
#include "nvme.h"
#include "myfs.h"
#include "common.h"
#include "ult.h"

#define N_HELPER (0)
#define USE_IO_URING_IOPOLL (0)

typedef long (*syscall_fn_t)(long, long, long, long, long, long, long);
static syscall_fn_t next_sys_call = NULL;

extern void (*debug_print4)(long, long, long, long, long);
extern int (*debug_printf)(const char *format, ...);
void load_debug(void);

typedef struct {
  int id;
  pthread_t pth;
  pthread_mutex_t mutex;
  pthread_cond_t cond;
  volatile int done;
  volatile int ready;
  volatile long arg[7];
  volatile long ret;
} helper_t;
static helper_t helpers[N_HELPER];

inline static void req_helper(int id, long a1, long a2, long a3, long a4, long a5, long a6, long a7) {
  helpers[id].done = false;
  helpers[id].arg[0] = a1;
  helpers[id].arg[1] = a2;
  helpers[id].arg[2] = a3;
  helpers[id].arg[3] = a4;
  helpers[id].arg[4] = a5;
  helpers[id].arg[5] = a6;
  helpers[id].arg[6] = a7;
  real_pthread_mutex_lock(&helpers[id].mutex);
  helpers[id].ready = true;
  real_pthread_cond_signal(&helpers[id].cond);
  real_pthread_mutex_unlock(&helpers[id].mutex);
};

void do_helper(void *arg) {
  helper_t *h = (helper_t *)arg;
  while (1) {
    real_pthread_mutex_lock(&(h->mutex));
    while (h->ready == false) {
      real_pthread_cond_wait(&h->cond, &h->mutex);
    }
    h->ready = false;
    real_pthread_mutex_unlock(&h->mutex);
    h->ret = next_sys_call(h->arg[0], h->arg[1], h->arg[2], h->arg[3], h->arg[4], h->arg[5], h->arg[6]);
    //if (debug_print)
    //debug_print(8, h->arg[0], h->ret);
    h->done = true;
  }
}

static char **hooked_filenames;
static int n_hooked_filenames;
static char *hooked_rocksdb_dir;

void
parse_hooked_filenames(char *s)
{
  n_hooked_filenames = 0;
  int start = 0;
  int i = 0;
  while (s[i] != '\0') {
    //printf("%c\n", s[i]);
    if (s[i] == ':') {
      n_hooked_filenames++;
      hooked_filenames = (char **)realloc(hooked_filenames, sizeof(char *) * n_hooked_filenames);
      printf("%s malloc(%d)\n", __func__, i+1);
      hooked_filenames[n_hooked_filenames-1] = malloc(i+1);
      //printf("%p, %p %d\n", hooked_filenames[n_hooked_filenames-1], &s[start], i);
      strncpy(hooked_filenames[n_hooked_filenames-1], &s[start], i);
      start = i + 1;
    }
    i++;
  }
  n_hooked_filenames++;
  hooked_filenames = (char **)realloc(hooked_filenames, sizeof(char *) * n_hooked_filenames);
  printf("%s malloc(%d)\n", __func__, i-start+1);
  hooked_filenames[n_hooked_filenames-1] = malloc(i-start+1);
  //printf("%p, %d\n", hooked_filenames[n_hooked_filenames-1], i);
  strncpy(hooked_filenames[n_hooked_filenames-1], &s[start], i-start);
}

static void init_hooked_filename() {
  char *hooked_filenames_str = getenv("HOOKED_FILENAMES");
  if (hooked_filenames_str) {
    parse_hooked_filenames(hooked_filenames_str);
    int i;
    for (i=0; i<n_hooked_filenames; i++) {
      printf("hooked_filename[%d] : %s\n", i, hooked_filenames[i]);
    }
  }
  hooked_rocksdb_dir = getenv("HOOKED_ROCKSDB_DIR");
  if (hooked_rocksdb_dir) {
    printf("hooked_rocksdb_dir : %s\n", hooked_rocksdb_dir);
  }  
}

static int is_hooked_filename(const char *filename)
{
  int ret = 0;
  if (hooked_filenames) {
    int i;
    for (i=0; i<n_hooked_filenames; i++) {
      ret |= (strncmp(hooked_filenames[i], filename, strlen(hooked_filenames[i])) == 0);
    }
  }
  if (hooked_rocksdb_dir) {
    const char sst_suffix[] = ".sst";
    const int sst_filename_len = 7;
    ret |= ((strncmp(hooked_rocksdb_dir, filename, strlen(hooked_rocksdb_dir)) == 0) &&
	    (strncmp(sst_suffix, filename + strlen(hooked_rocksdb_dir) + sst_filename_len, strlen(sst_suffix)) == 0));
  }
  return ret;
}

static struct iocb *cur_aios[1024];
static int cur_aio_wp;
static int cur_aio_rp;
static int cur_aio_max;

#define MAX_HOOKFD (1024)
int hookfds[MAX_HOOKFD];
size_t cur_pos[MAX_HOOKFD];

static inline void
read_impl(int hookfd, loff_t len, loff_t pos, char *buf)
{
  int core_id = ult_core_id();
  //printf("%s fd=%d len=%d pos=%lld core=%d\n", __func__, hookfd, len, pos, core_id);

  myfs_increment_ref(hookfd);
  
  size_t file_size = myfs_get_size(hookfd);
  if (pos + len > file_size) {
    len = file_size - pos;
  }
#if 0
  if (pos % 512 != 0) {
    printf("error len %lu pos %lu %lu\n", len, pos, cur_pos[hookfd]);
    assert(0);
  }
#else
  assert(pos % 512 == 0);
#endif
  int blksz = ((len % BLKSZ != 0) || (pos % BLKSZ != 0)) ? 512 : BLKSZ;
  if ((pos % BLKSZ) + len <= BLKSZ) {
    blksz = len;
  }
  int j;
#if 0
  for (j=0; j<len; j+=blksz) {
    int64_t lba = myfs_get_lba(hookfd, pos + j, 0);
    int rid = nvme_read_req(lba, DIV_CEIL(blksz,512), core_id, MIN(blksz, len - j), buf + j);
    while (1) {
      if (nvme_check(rid))
	break;
      ult_yield();
    }
  }
#else
  int rid[READ_DEG];
  j = 0;
  while (j<len) {
    int rq_k;
    int k;
    for (rq_k=0; rq_k<READ_DEG; rq_k++) {
      if (j >= len)
	break;
      int64_t lba = myfs_get_lba(hookfd, pos + j, 0);
      rid[rq_k] = nvme_read_req(lba, DIV_CEIL(blksz,512), core_id, MIN(blksz, len - j), buf + j);
      j += blksz;
    }
    for (k=0; k<rq_k; k++) {
      while (1) {
	if (nvme_check(rid[k]))
	  break;
	ult_yield();
      }
    }
  }
#endif
  
  myfs_decrement_ref(hookfd);
  //printf("%s done! fd=%d len=%d pos=%lld core=%d\n", __func__, hookfd, len, pos, core_id);
}

void
write_impl(int hookfd, loff_t len, loff_t pos, char *buf)
{

  int core_id = ult_core_id();
  assert(pos % 512 == 0);
  
  myfs_increment_ref(hookfd);
  int blksz = ((len % BLKSZ != 0) || (pos % BLKSZ != 0)) ? 512 : BLKSZ;
  
  int j;
  //printf("%s %d hookfd=%d len=%ld pos=%ld %d\n", __func__, __LINE__, hookfd, len, pos, blksz);
#if 0
  for (j=0; j<len; j+=blksz) {
    int64_t lba = myfs_get_lba(hookfd, pos + j, 1);
    //printf("%s %d hookfd=%d len=%lu %lu pos =%lu\n", __func__, __LINE__, hookfd, len, j, pos);
    int rid = nvme_write_req(lba, DIV_CEIL(blksz,512), core_id, MIN(blksz, len - j), buf + j);
    while (1) {
      if (nvme_check(rid))
	break;
      ult_yield();
    }
  }
#else
  int rid[WRITE_DEG];
  j = 0;
  while (j<len) {
    int rq_k;
    int k;
    for (rq_k=0; rq_k<WRITE_DEG; rq_k++) {
      int64_t lba = myfs_get_lba(hookfd, pos + j, 1);
      rid[rq_k] = nvme_write_req(lba, DIV_CEIL(blksz,512), core_id, MIN(blksz, len - j), buf + j);
      j += blksz;
      if (j >= len)
	break;
    }
    for (k=0; k<rq_k; k++) {
      while (1) {
	if (nvme_check(rid[k]))
	  break;
	ult_yield();
      }
    }
  }
#endif
  
  //printf("%s done %d hookfd=%d len=%ld pos=%ld %d\n", __func__, __LINE__, hookfd, len, pos, blksz);
  myfs_decrement_ref(hookfd);
}

static struct submitter sub[N_CORE];

static inline
void __io_uring_bottom(int core_id, int sqe_id)
{
  iouring_submit(&sub[core_id], 1);
  while (1) {
    iouring_getevents(&sub[core_id]);
    iouring_check(&sub[core_id]);
    if (sub[core_id].done_flag[sqe_id])
      break;
    ult_yield();
  }
}

static inline
void __io_uring_read(int fd, char *buf, size_t count, loff_t pos)
{
  int core_id = ult_core_id();
  //printf("Read core=%d fd=%d count=%lu pos=%lu\n", core_id, fd, count, pos);
  int sqe_id = iouring_read(&sub[core_id], fd, buf, count, pos);
  __io_uring_bottom(core_id, sqe_id);
  //printf("Read Done core=%d fd=%d count=%lu pos=%lu\n", core_id, fd, count, pos);
}

static inline
void __io_uring_write(int fd, char *buf, size_t count, loff_t pos)
{
  int core_id = ult_core_id();
  //printf("Write core=%d fd=%d count=%lu pos=%lu\n", core_id, fd, count, pos);
  int sqe_id = iouring_write(&sub[core_id], fd, buf, count, pos);
  __io_uring_bottom(core_id, sqe_id);
  //printf("Write Done core=%d fd=%d count=%lu pos=%lu\n", core_id, fd, count, pos);
}

static inline int
hook_openat(long a1, long a2, long a3,
	    long a4, long a5, long a6,
	    long a7)
{
  int dfd = a2; // dir. fd is not used.
  const char *filename = (const char *)a3;
  int flags = a4;
  mode_t mode = a5;

  int ret = next_sys_call(a1, a2, a3, a4, a5, a6, a7);
  if (is_hooked_filename(filename)) {
#if DEBUG_HOOK_FILE
    printf("hooked file: fd=%d dfd=%d filename=%s flags=%o mode=%o\n", ret, dfd, filename, flags, mode);
#endif
    if (ret < MAX_HOOKFD) {
#if USE_IO_URING
      hookfds[ret] = 1;
#else
      hookfds[ret] = myfs_open(filename);
#endif
      cur_pos[ret] = 0;
    } else {
      printf("error: reached upper limit of opened files.\n");
      assert(0);
    }
  }
  return ret;
}

static inline int
hook_futex(long a1, long a2, long a3,
	   long a4, long a5, long a6,
	   long a7)
{
  uint32_t *uaddr = (uint32_t *)a2;
  int futex_op = a3;
  uint32_t val = (uint32_t)a4;
  //if ((futex_op & FUTEX_WAIT) && (a6 != 0xdeadcafe)) {
  if ((a6 != 0xdeadcafe) && ((futex_op == FUTEX_WAIT) || (futex_op == (FUTEX_WAIT | FUTEX_PRIVATE_FLAG)))) {
    while (1) {
      if (*uaddr != val)
	return 0;
      ult_yield();
    }
  } else {
    return next_sys_call(a1, a2, a3, a4, a5, a6, a7);
  }
}

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

extern int mylib_initialized;

long hook_function(long a1, long a2, long a3,
		   long a4, long a5, long a6,
		   long a7)
{

  /*
  if (debug_print) {
    debug_print(1, a1, 9999);
  }
  */
  /*
  if ((a1 != 1) && (a1 != 202) && (a1 != 24) && (a1 != 12)) {
    printf("call %ld\n", a1);
  }
  */
  
  if (is_ult()) {

    /*
    if ((a1 != 1) && (a1 != 17) && (a1 != 18)) {
      printf("call %ld %ld\n", a1, abt_id);
    }
    */

    if (debug_print) {
      debug_print(1, a1, ult_id());
    }
    
    switch (a1) {
    case SYS_openat:
      return hook_openat(a1, a2, a3, a4, a5, a6, a7);
    case SYS_close:
      {
	int fd = a2;
	if (hookfds[fd] >= 0) {
#if USE_IO_URING
#else
	  myfs_close(hookfds[fd]);
#endif
	  //printf("close for mylib: fd=%d hookfd=%d\n", fd, hookfds[fd]);
	  hookfds[fd] = NOT_USED_FD;
	}
	return next_sys_call(a1, a2, a3, a4, a5, a6, a7);
      }
    case SYS_read:
      {
	int fd = a2;
	char *buf = (char *)a3;
	size_t count = a4;
	int hookfd = hookfds[fd];
	//printf("read %d %d\n", a2, hookfd);
	if (hookfd >= 0) {
#if USE_IO_URING
	  __io_uring_read(fd, buf, count, -1);
#else
	  //ult_mutex_lock();
	  read_impl(hookfd, count, cur_pos[fd], buf);
	  cur_pos[fd] += count;
	  //ult_mutex_unlock();
#endif
	  return count;
	} else {
	  return next_sys_call(a1, a2, a3, a4, a5, a6, a7);
	}
      }
    case 17: // pread64
      {
	int fd = a2;
	char *buf = (char *)a3;
	size_t count = a4;
	loff_t pos = a5;
	int hookfd = hookfds[fd];
	//printf("pread64 %s %d hookfd=%d count=%lu pos=%lu\n", __func__, __LINE__, hookfd, count, pos);
	if (hookfd >= 0) {
#if USE_IO_URING
	  __io_uring_read(fd, buf, count, pos);
#else
#if 1
	  read_impl(hookfd, count, pos, buf);
#else
	  {
	    int64_t lba = myfs_get_lba(hookfd, pos, 0);
	    int i_core = 0;
	    int rid = nvme_read_req(lba, 1, i_core, 64, buf);
	    while (1) {
	      ult_yield();
	      if (nvme_check(rid))
		break;
	    }
	  }
#endif
#endif
	  return count;
	} else {
	  return next_sys_call(a1, a2, a3, a4, a5, a6, a7);
	}
      }
    case SYS_write:
      {
	int fd = a2;
	int hookfd = hookfds[a2];
	char *buf = (char *)a3;
	loff_t len = a4;
	if (hookfd >= 0) {
#if USE_IO_URING
	  __io_uring_write(fd, buf, len, -1);
	  //return next_sys_call(a1, a2, a3, a4, a5, a6, a7);
#else
	  //ult_mutex_lock();
	  write_impl(hookfd, len, cur_pos[a2], buf);
	  cur_pos[a2] += len;
	  //ult_mutex_unlock();
#endif
	  return len;
	} else {
	  return next_sys_call(a1, a2, a3, a4, a5, a6, a7);
	}
      }
    case 18: // pwrite64
      {
	int fd = a2;
	int hookfd = hookfds[a2];
	char *buf = (char *)a3;
	loff_t len = a4;
	loff_t pos = a5;
	//printf("pwrite64 %d hookfd=%d, len=%ld, pos=%ld\n", a2, hookfd, len, pos);
	if (hookfd >= 0) {
	  //printf("pwrite64 %d hookfd=%d, len=%ld, pos=%ld\n", a2, hookfd, len, pos);
#if USE_IO_URING
	  __io_uring_write(fd, buf, len, pos);
	  //return next_sys_call(a1, a2, a3, a4, a5, a6, a7);
#else
	  write_impl(hookfd, len, pos, buf);
#endif
	  return len;
	} else {
	  return next_sys_call(a1, a2, a3, a4, a5, a6, a7);
	}
      }
    case 262: // fstat
#if USE_IO_URING
      return next_sys_call(a1, a2, a3, a4, a5, a6, a7);
#else
      if (((int32_t)a2 >= 0) && (hookfds[a2] >= 0)) {
	uint64_t sz = myfs_get_size(hookfds[a2]);
	struct stat *statbuf = (struct stat*)a4;
	statbuf->st_size = sz;
	statbuf->st_blocks = 512;
	statbuf->st_blksize = sz / 512;
	//printf("fstat: file size = %ld fd=%ld\n", sz, a2);
	return 0;
      } else {
	return next_sys_call(a1, a2, a3, a4, a5, a6, a7);
      }
#endif
    case 230: // nanosleep
      return hook_clock_nanosleep(a2, a3, (struct timespec *)a4, (struct timespec *)a5);
    case 87: // unlink
      {
	char *pathname = (char *)a2;
#if USE_IO_URING
#else
	myfs_unlink(pathname);
#endif
	return next_sys_call(a1, a2, a3, a4, a5, a6, a7);
      }
    case 270: // select
      {
	struct timespec *ats = (struct timespec *) a6;
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC_COARSE, &ts);
	ts.tv_sec += ats->tv_sec;
	ts.tv_nsec += ats->tv_nsec;
	int ret;
	while (1) {
	  struct timespec zts = {.tv_sec = 0, .tv_nsec = 0};
	  if (a6) {
	    struct timespec ts2;
	    clock_gettime(CLOCK_MONOTONIC_COARSE, &ts2);
	    double diff_nsec = (ts2.tv_sec - ts.tv_sec) * 1e9 + (ts2.tv_nsec - ts.tv_nsec);
	    if (diff_nsec > 0)
	      return 0;
	  }
	  ret = next_sys_call(a1, a2, a3, a4, a5, (long)&zts, a7);
	  if (ret) {
	    return ret;
	  }
	  ult_yield();
	}
      }
    case 441: // epoll_pwait2
      {
	struct timespec tsz = {.tv_sec = 0, .tv_nsec = 0};
	struct timespec *ts = (struct timespec *) a5;
	if (ts) {
	  struct timespec tsc;
	  clock_gettime(CLOCK_MONOTONIC_COARSE, &tsc);
	  ts->tv_sec += tsc.tv_sec;
	  ts->tv_nsec += tsc.tv_nsec;
	}
	while (1) {
	  int ret = next_sys_call(a1, a2, a3, a4, (long)&tsz, a6, a7);
	  if (ret > 0) {
	    return ret;
	  }
	  if (ts) {
	    struct timespec ts2;
	    clock_gettime(CLOCK_MONOTONIC_COARSE, &ts2);
	    double diff_nsec = (ts2.tv_sec - ts->tv_sec) * 1e9 + (ts2.tv_nsec - ts->tv_nsec);
	    if (diff_nsec > 0)
	      return 0;
	  }
	  ult_yield();
	}
      }
#if 1
    case 202: // futex
      return hook_futex(a1, a2, a3, a4, a5, a6, a7);
#endif // futex

#if 0 // AIO
    case 208: // io_getevents
      int min_nr = a3;
      int nr = a4;
      int completed = 0;
      //printf("hogehoge\n");
      struct io_event *events = (struct io_event *)a5;
      while (completed < nr) {
	if (cur_aio_wp == cur_aio_rp) {
	  break;
	}
	//printf("cur_aio_wp = %d, cur_aio_rp = %d\n", cur_aio_wp, cur_aio_rp);
	int rid = cur_aios[cur_aio_rp]->aio_reserved2;
	int fd = cur_aios[cur_aio_rp]->aio_fildes;
	uint64_t pos = cur_aios[cur_aio_rp]->aio_offset;
	int64_t lba = myfs_get_lba(hookfds[fd], pos, 0);
	//printf("io_getevents fd=%d cid=%d pos=%lu lba=%u qid=%d\n", fd, cid, pos, lba, qid);
	while (1) {
	  if (nvme_check(rid))
	    break;
	  ult_yield();
	}
	//printf("%s %d completed%d\n", __func__, __LINE__, completed);
	events[completed].data = cur_aios[cur_aio_rp]->aio_buf;
	events[completed].obj = (uint64_t)cur_aios[cur_aio_rp];
	//printf("io_submitted callback %lx rp %d\n", cur_aios[cur_aio_rp], cur_aio_rp);
	if (0) {
	  struct iocb *cb = (void*)events[completed].obj;
	  struct slab_callback *callback = (void*)cb->aio_data;
	  int op = cb->aio_lio_opcode;
	  printf("io_getevents callback %p rd=%d rid=%d\n", callback, op == IOCB_CMD_PREAD, rid);
	  //debug_item(111, cb->aio_data);
	}
	events[completed].res = cur_aios[cur_aio_rp]->aio_nbytes;
	events[completed].res2 = 0;
	completed++;
	cur_aio_rp = (cur_aio_rp + 1) % cur_aio_max;
	if (completed == nr)
	  break;
      }
      //printf("completed %d\n", completed);
      return completed;
    case 209: // io_submit
      int core_id = ult_core_id();
      struct iocb **ios = (struct iocb **)a4;
      int n_io = a3;
      int i;
      for (i=0; i<n_io; i++) {
	if ((cur_aio_wp + 1) % cur_aio_max == cur_aio_rp) {
	  break;
	}

	int fd = ios[i]->aio_fildes;
	int op = ios[i]->aio_lio_opcode;
	//printf("io_submitted %p callback %lx wp %d rd?=%d\n", ios[i], ios[i]->aio_data, cur_aio_wp, op == IOCB_CMD_PREAD);
	char *buf = (char *)ios[i]->aio_buf;
	uint64_t len = ios[i]->aio_nbytes;
	uint64_t pos = ios[i]->aio_offset;
	int blksz = BLKSZ;
	assert(hookfds[fd] >= 0);
	
	if (op == IOCB_CMD_PREAD) {
	  int64_t lba = myfs_get_lba(hookfds[fd], pos, 0);
	  int rid = nvme_read_req(lba, blksz/512, core_id, MIN(blksz, len), buf);
	  //printf("io_submit read op=%d fd=%d, sz=%ld, pos=%ld lba=%d rid=%d\n", op, fd, len, pos, lba, rid);
	  ios[i]->aio_reserved2 = rid;
#if 0
	  while (1) {
	    if (nvme_check(rid))
	      break;
	    ult_yield();
	  }
	  ios[i]->aio_reserved2 = JUST_ALLOCATED;
#endif
	}
	if (op == IOCB_CMD_PWRITE) {
	  int64_t lba = myfs_get_lba(hookfds[fd], pos, 1);
	  int rid = nvme_write_req(lba, blksz/512, core_id, MIN(blksz, len), buf);
	  //printf("io_submit write op=%d fd=%d, sz=%ld, pos=%ld lba=%d rid=%d\n", op, fd, len, pos, lba, rid);
	  ios[i]->aio_reserved2 = rid;
#if 0
	  //debug_item(114, ios[i]->aio_data);
	  while (1) {
	    if (nvme_check(rid))
	      break;
	    ult_yield();
	  }
	  ios[i]->aio_reserved2 = JUST_ALLOCATED;
	  //debug_item(115, ios[i]->aio_data);
#endif
	  
	}
	//printf("cur_aio_wp = %d\n", cur_aio_wp);
	cur_aios[cur_aio_wp] = ios[i];
	//debug_item(112, ios[i]->aio_data);
	cur_aio_wp = (cur_aio_wp + 1) % cur_aio_max;
      }
      return i;//return next_sys_call(a1, a2, a3, a4, a5, a6, a7);
    case 206: // io_setup
      cur_aio_max = a2;
      cur_aio_wp = 0;
      cur_aio_rp = 0;
      cur_aio_max = a2;
      printf("io_setup %d %p %p\n", cur_aio_max, (void *)a3, cur_aios);
      return 0;
    case 207: // io_destroy
      printf("io_destroy %ld %p\n", a2, (void *)a3);
      return 0;
#endif // AIO
    default:
      return next_sys_call(a1, a2, a3, a4, a5, a6, a7);
      /*
      req_helper(abt_id, a1, a2, a3, a4, a5, a6, a7);
      while (1) {
	if (helpers[abt_id].done)
	  break;
	ult_yield();
	//uint64_t pre_id;
	//int ret = ABT_self_get_pre_id(&pre_id);
      }
      return helpers[abt_id].ret;
      */
    }
  } else {
#if 0
    if (a1 == 202) {
      int op = a3;
      if (op & 1024) {
	op ^= 1024;
      }
      return next_sys_call(202, a2, op, a4, a5, a6, a7);
    }
#endif
    /*
    if ((a1 == 1) || (a1 == 18)) {
      printf("outside write %ld %ld\n", a1, a2);
    }
    */
    return next_sys_call(a1, a2, a3, a4, a5, a6, a7);
  }
}




int __hook_init(long placeholder __attribute__((unused)),
		void *sys_call_hook_ptr)
{

  int i;
  init_hooked_filename();
  
  for (i=0; i<MAX_HOOKFD; i++) {
    hookfds[i] = NOT_USED_FD;
  }
  
  load_debug();

  for (i=0; i<N_HELPER; i++) {
    helpers[i].id = i;
    real_pthread_mutex_init(&helpers[i].mutex, NULL);
    real_pthread_cond_init(&helpers[i].cond, NULL);
    real_pthread_create(&helpers[i].pth, NULL, (void *(*)(void *))do_helper, &helpers[i]);
  }

  next_sys_call = *((syscall_fn_t *) sys_call_hook_ptr);
  *((syscall_fn_t *) sys_call_hook_ptr) = hook_function;

#if USE_IO_URING
  for (i=0; i<N_CORE; i++) {
    //printf("%s %d %p %d\n", __func__, __LINE__, ring, i);
#if USE_IO_URING_IOPOLL
    iouring_setup(&sub[i], 1);
#else
    iouring_setup(&sub[i], 0);
#endif
    //printf("%s %d\n", __func__, __LINE__);
  }
#else
  char *myfs_superblock_path = getenv("MYFS_SUPERBLOCK_PATH");
  assert(myfs_superblock_path);
  myfs_mount(myfs_superblock_path);
  
  nvme_init();
#endif
  
  return 0;

}

__attribute__((destructor(0xffff))) static void
hook_deinit()
{
#if USE_IO_URING
#else
  myfs_umount();
#endif
}

