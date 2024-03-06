#include <stdint.h>
#include "nvme.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include "common.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <assert.h>
#include <sys/mman.h>
#include "myfs.h"

superblock_t *superblock;

void
myfs_mount(char *myfs_superblock)
{
  int superblock_fd = open(myfs_superblock, O_RDWR);
  if (superblock_fd < 0) {
    perror("open");
  }
  size_t page_size = getpagesize();
  struct stat statbuf;
  fstat(superblock_fd, &statbuf);
  uint64_t mapped_size = statbuf.st_size;
  mapped_size = (mapped_size / page_size) * page_size;
  printf("sizeof(superblock_t)=%ld, mmapped_size=%ld\n", sizeof(superblock_t), mapped_size);
  superblock = (superblock_t *)mmap(0, mapped_size, PROT_READ|PROT_WRITE, MAP_SHARED, superblock_fd, 0);
  if (superblock == MAP_FAILED)
    perror("mmap superblock file");
}

void
append(int ssd_fd, int i)
{
  char buf[DEG][BUF_SIZE];
  int rid[DEG];
  int j;
  int n_block = superblock->file[i].n_block;
  int i_block;
  for (i_block=0; i_block<n_block; i_block++) {
    uint64_t blk = superblock->file[i].block[i_block];
    uint64_t lba_base = blk * MYFS_BLOCK_SIZE / 512;
    uint64_t offset;
    for (offset=0; offset<MYFS_BLOCK_SIZE / 512; offset+=8*DEG) {
      for (j=0; j<DEG; j++) {
	rid[j] = nvme_read_req(lba_base + offset + 8*j, BUF_SIZE/512, 0, BUF_SIZE, buf[j]);
      }
    }
    for (j=0; j<DEG; j++) {
      while (1)
	if (nvme_check(rid[j]))
	  break;
      int ret = write(ssd_fd, buf[j], BUF_SIZE);
    }
  }
}
  
int
main(int argv, char **argc)
{
  char my_backup_path[256];
  char myfs_superblock_path[256];
  assert(argv == 2);

  printf("backup @ %s\n", argc[1]);

  sprintf(my_backup_path, "%s/my_backup.dat", argc[1]);
  sprintf(myfs_superblock_path, "%s/myfs_superblock", argc[1]);
  
  nvme_init();
  int ssd_fd = open(my_backup_path, O_RDWR|O_CREAT, 0);
  {
    int i;
    myfs_mount(myfs_superblock_path);
    for (i=0; i<MYFS_MAX_FILES; i++) {
      if (superblock->file[i].name[0] != '\0') {
	printf("%d %s %lu\n", i, superblock->file[i].name, superblock->file[i].n_block);
	append(ssd_fd, i);
      }
    }
  }
  close(ssd_fd);
}
