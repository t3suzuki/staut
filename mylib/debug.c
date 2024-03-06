#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>


int
__debug_printf(const char *format, ...)
{
  va_list ap;
  va_start(ap, format);
  int result = printf(format, ap);
  va_end(ap);
  return result;
}

void
__debug_print4(long id, long a2, long a3, long a4, long a5)
{
  switch (id) {
  case 7:
    printf("pwrite64 fd=%ld, wbuf=%lx, write_sz=%ld, disk_pos=%ld\n", a2, a3, a4, a5);
    if (a4 != 1048576) {
      unsigned char *wbuf = (unsigned char *)a2;
      int i;
      for (i=0; i<512; i++) {
	printf("%02x ", wbuf[i]);
	if (i % 16 == 15)
	  printf("\n");
      }
    }
    break;
  }
}

static char syscall_name[512][32];

void
__debug_print(long id, long a, long tid)
{
  if (id > 900) {
    printf("%ld %ld\n", id, a);
    return;
  }
  //sleep(1);
  switch (id) {
  case 1:
    printf("req_helper %ld %s @ tid %ld\n", a, syscall_name[a], tid);
    break;
  case 2:
    printf("helper done %ld @ ret %ld\n", a, tid);
    break;
  case 3:
    //printf("Current tid %ld\n", tid);
    break;
  case 5:
    printf("Pre-syscall call_id = %ld   (%ld)\n", a, tid);
    break;
  case 6:
    printf("Post-syscall call_id = %ld   (%ld)\n", a, tid);
    break;
  case 666:
    printf("sleep = %ld %ld\n", a, tid);
    break;
  case 874:
    //printf("read64 = %ld %ld\n", a, tid);
    break;
  case 876:
    //printf("read wait = %ld %ld\n", a, tid);
    break;
  case 886:
    printf("%c %ld\n", ((char *)a)[0], tid);
    break;
  case 885:
    printf("%ld %ld\n", a, tid);
    break;
  case 883:
    printf("read fd = %ld (pos = %ld)\n", a, tid);
    break;
  case 882:
    //printf("read_done %ld\n", a);
    break;
  case 884:
    printf("hookfd = %ld %ld\n", a, tid);
    break;
  case 888:
    printf("time1 %ld   (%f)\n", a, (double)tid * 1e-9);
    break;
  case 870:
    printf("select %ld  %ld\n", a, tid);
    break;
  case 871:
    printf("sleep arg tv_sec %ld  tv_nsec %ld\n", a, tid);
    break;
  case 889:
    printf("time2 %ld   (%f)\n", a, (double)tid * 1e-9);
    break;
  case 520:
    printf("debug nvme req lba %ld (%d) cid %ld\n", a, (unsigned char)a & 0xff, tid);
    break;
  case 521:
    printf("debug nvme cmp %ld %ld\n", a, tid);
    break;
  case 522:
    printf("debug nvme cmp2 %ld %ld\n", a, tid);
    break;
  case 59:
    printf("exec %s\n", (char *)a);
    break;
  }
}


#define def_syscall_name(num, name) (memcpy(syscall_name[num], name, strlen(name)))

__attribute__((constructor(0xffff))) static void
__debug_init()
{
  def_syscall_name(12, "brk");
  def_syscall_name(257, "openat");
  def_syscall_name(262, "newfstatat");
  def_syscall_name(3, "close");
  def_syscall_name(1, "write");
  def_syscall_name(17, "pread64");
  def_syscall_name(18, "pwrite64");
  def_syscall_name(426, "io_uring_enter");
  def_syscall_name(75, "fdatasync");
  def_syscall_name(0, "read");
  def_syscall_name(77, "ftruncate");
  def_syscall_name(8, "lseek");
  def_syscall_name(72, "fcntl");
  
  def_syscall_name(83, "mkdir");
  def_syscall_name(84, "rmdir");
  def_syscall_name(217, "getdents64");
  def_syscall_name(82, "rename");
  def_syscall_name(87, "unlink");
  
  def_syscall_name(99, "sysinfo");
  def_syscall_name(10, "mprotect");
  
  def_syscall_name(9, "mmap");
  def_syscall_name(25, "mremap");
  
  def_syscall_name(230, "clock_nanosleep");
  def_syscall_name(208, "io_getevents");
  def_syscall_name(209, "io_submit");
  def_syscall_name(206, "io_setup");
  def_syscall_name(207, "io_destroy");
  def_syscall_name(285, "fallocate");
}
