#include <stdio.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <assert.h>

//#define MY_NEW_FILENAME ("my_new_file")
#define MY_NEW_FILENAME ("/dev/nvme0n1")
#define HOOKED_FILENAME ("myfile")


#define MAX_HOOKFD (1024)

typedef long (*syscall_fn_t)(long, long, long, long, long, long, long);

static syscall_fn_t next_sys_call = NULL;


int is_hooked_filename(const char *filename)
{
  char hooked_filename[] = HOOKED_FILENAME;
  return (strncmp(filename, hooked_filename, strlen(hooked_filename)) == 0);
}

int hookfds[MAX_HOOKFD];
static long hook_function(long a1, long a2, long a3,
			  long a4, long a5, long a6,
			  long a7)
{
	switch (a1) {
	case SYS_openat:
	  const char *filename = (const char *)a3;
	  int ret = next_sys_call(a1, a2, a3, a4, a5, a6, a7);
	  if (is_hooked_filename(filename)) {
	    printf("is_hooke_file!\n");
	    hookfds[ret] = open(MY_NEW_FILENAME, O_RDWR);
	    assert(hookfds[ret] >= 0);
	  }
	  return ret;
	case SYS_read: //pread
	case SYS_write: //pwrite
	case 17: //pread
	case 18: //pwrite
	  int fd = a2;
	  if (hookfds[fd] >= 0) {
	    //printf("output from hook_function: syscall number %ld\n", a1);
	    return next_sys_call(a1, hookfds[fd], a3, a4, a5, a6, a7);
	  } else {
	    return next_sys_call(a1, a2, a3, a4, a5, a6, a7);
	  }
	}
	return next_sys_call(a1, a2, a3, a4, a5, a6, a7);
}

int __hook_init(long placeholder __attribute__((unused)),
		void *sys_call_hook_ptr)
{
  //printf("output from __hook_init: we can do some init work here\n");
	int i;
	for (i=0; i<MAX_HOOKFD; i++) {
	  hookfds[i] = -1;
	}

	next_sys_call = *((syscall_fn_t *) sys_call_hook_ptr);
	*((syscall_fn_t *) sys_call_hook_ptr) = hook_function;

	return 0;
}
