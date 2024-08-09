#include <assert.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

//#define FILENAME ("/dev/nvme0n1")
#define FILENAME ("myfile")


char buf[4096];
int
main()
{
  int fd;
  int i;
  fd = open(FILENAME, O_RDWR);
  assert(fd > 0);
  for (i=0; i<4096; i++) {
    buf[i] = i;
  }
  //buf[0] = 99;
  pwrite(fd, buf, 4096, 0);
  pread(fd, buf, 4096, 0);
  for (i=0; i<4096; i++) {
    if (buf[i] != (char)i)
      printf("%d != %d\n", i, buf[i]);
  }
  
}
