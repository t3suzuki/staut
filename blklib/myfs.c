#define _GNU_SOURCE
#include <fcntl.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <errno.h>
#include <assert.h>
#include "nvme.h"
#include "common.h"
#include "ult.h"
#include "abt.h"
#include "myfs.h"

ult_mutex myfs_file_mutex[MYFS_MAX_FILES];
ult_mutex myfs_mutex;

myfs_file_state_t myfs_file_states[MYFS_MAX_FILES];

superblock_t *superblock;

static int superblock_fd = -1;

static void
myfs_superblock_init()
{
  int i, j;
  printf("%s superblock_size = %lu\n", __func__, sizeof(superblock_t));
  for (i=0; i<MYFS_MAX_FILES; i++) {
    superblock->file[i].name[0] = '\0';
    for (j=0; j<MYFS_MAX_BLOCKS_PER_FILE; j++) {
      superblock->file[i].block[j] = INACTIVE_BLOCK;
    }
  }
  for (i=0; i<NUM_BLOCKS-1; i++) {
    superblock->free_blocks[i] = i;
  }
  superblock->free_blocks_rp = 0;
  superblock->free_blocks_wp = NUM_BLOCKS - 1;
  superblock->magic = MYFS_MAGIC;

}


static int
myfs_used_blocks()
{
  int rp = superblock->free_blocks_rp;
  int wp = superblock->free_blocks_wp;
  if (rp < wp) {
    return rp + NUM_BLOCKS - wp - 1;
  } else {
    return rp - wp - 1;
  }
}

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

  if (superblock->magic != MYFS_MAGIC) {
    myfs_superblock_init();
  }

  int i;
  for (i=0; i<MYFS_MAX_FILES; i++) {
    myfs_file_states[i].flags = 0;
    myfs_file_states[i].ref_count = 0;
    ult_mutex_create(&myfs_file_mutex[i]);
  }
  ult_mutex_create(&myfs_mutex);
}

void
__myfs_unlink(int fd)
{
  uint64_t j;
  //printf("%s %d %lu wp=%u rp=%u\n", __func__, fd, superblock->file[fd].n_block, superblock->free_blocks_wp, superblock->free_blocks_rp);
  for (j=0; j<superblock->file[fd].n_block; j++) {
    int old_val;
    while (1) {
      old_val = superblock->free_blocks_wp;
      int new_val = (old_val + 1) % NUM_BLOCKS;
      if (__sync_bool_compare_and_swap(&superblock->free_blocks_wp, old_val, new_val))
	break;
    }
    superblock->free_blocks[old_val] = superblock->file[fd].block[j];
    superblock->file[fd].block[j] = INACTIVE_BLOCK;
  }
  superblock->file[fd].name[0] = '\0';
  superblock->file[fd].n_block = 0;
  //printf("%s %d %lu wp=%u rp=%u\n", __func__, fd, superblock->file[fd].n_block, superblock->free_blocks_wp, superblock->free_blocks_rp);
  
  myfs_file_states[fd].ref_count = 0;
  myfs_file_states[fd].flags = 0;
}

void
__myfs_try_unlink(int fd)
{
  myfs_file_states[fd].flags |= MYFS_FILE_UNLINKED;
  if (myfs_file_states[fd].ref_count == 0) {
    __myfs_unlink(fd);
  }
}

void
myfs_unlink_fd(int fd)
{
  ult_mutex_lock((ult_mutex *)&myfs_mutex);
  __myfs_try_unlink(fd);
  ult_mutex_unlock((ult_mutex *)&myfs_mutex);  
}

void
myfs_unlink(const char *filename)
{
  int i;
  ult_mutex_lock((ult_mutex *)&myfs_mutex);
  for (i=0; i<MYFS_MAX_FILES; i++) {
    if (strncmp(filename, superblock->file[i].name, strlen(filename)) == 0) {
      __myfs_try_unlink(i);
    }
  }  
  ult_mutex_unlock((ult_mutex *)&myfs_mutex);
}


int
myfs_open(const char *filename)
{
  int i;
  int empty_i = -1;
  
  ult_mutex_lock((ult_mutex *)&myfs_mutex);
  for (i=0; i<MYFS_MAX_FILES; i++) {
    //printf("%d %s check %s %s\n", i, __func__, superblock->file[i].name, filename);
    if (strncmp(filename, superblock->file[i].name, strlen(filename)) == 0) {
      myfs_file_states[i].flags = MYFS_FILE_OPENED;
      ult_mutex_unlock((ult_mutex *)&myfs_mutex);
#if DEBUG_HOOK_FILE
      printf("%s found %s fileid=%d\n", __func__, filename, i);
#endif
      return i;
    }
    if ((empty_i == -1) && (superblock->file[i].name[0] == '\0')) {
      empty_i = i;
    }
  }
#if DEBUG_HOOK_FILE
  printf("%s file not found. new fileid=%d for %s\n", __func__, empty_i, filename);
#endif
  strncpy(superblock->file[empty_i].name, filename, strlen(filename)+1);
  superblock->file[empty_i].n_block = 0;
  myfs_file_states[empty_i].flags = MYFS_FILE_OPENED;
  ult_mutex_unlock((ult_mutex *)&myfs_mutex);
  
  return empty_i;
}


int64_t
myfs_get_lba(int i, uint64_t offset, int write) {
  int i_block = offset / MYFS_BLOCK_SIZE;
  //printf("%s %d offset=%ld write=%d block=%d n_block=%d\n", __func__, i, offset, write, superblock->file[i].block[i_block], superblock->file[i].n_block);
  if (write > 0) {
    ult_mutex_lock((ult_mutex *)&myfs_file_mutex[i]);
    //printf("a fd=%d, i_block %d, block=%d rp=%d wp=%d\n", i, i_block, superblock->file[i].block[i_block], superblock->free_blocks_rp, superblock->free_blocks_wp);
    if (superblock->file[i].block[i_block] == INACTIVE_BLOCK) {
      {
	int old_val;
	while (1) {
	  old_val = superblock->free_blocks_rp;
	  int new_val = (old_val + 1) % NUM_BLOCKS;
	  if (__sync_bool_compare_and_swap(&superblock->free_blocks_rp, old_val, new_val))
	    break;
	}
	superblock->file[i].block[i_block] = superblock->free_blocks[old_val];
      }
      superblock->file[i].n_block++;
      //printf("a fd=%d, i_block %d, block=%d rp=%d wp=%d\n", i, i_block, superblock->file[i].block[i_block], superblock->free_blocks_rp, superblock->free_blocks_wp);
    }
    ult_mutex_unlock((ult_mutex*)&myfs_file_mutex[i]);
  }
  //printf("%s fileid=%d i_block %d block %d offset %ld\n", __func__, i, i_block, superblock->file[i].block[i_block], (uint64_t)superblock->file[i].block[i_block] * MYFS_BLOCK_SIZE);
  if (superblock->file[i].block[i_block] == INACTIVE_BLOCK) {
    //fflush(0);
    //printf("%s fileid=%d i_block %d block %d offset %ld\n", __func__, i, i_block, superblock->file[i].block[i_block], (uint64_t)superblock->file[i].block[i_block] * MYFS_BLOCK_SIZE); fflush(0);
    assert(superblock->file[i].block[i_block] != INACTIVE_BLOCK);
  }
  int64_t lba = ((uint64_t)superblock->file[i].block[i_block] * MYFS_BLOCK_SIZE + (offset % MYFS_BLOCK_SIZE)) / 512;
  return lba;
}


void
myfs_sync()
{
  fsync(superblock_fd);
#if DEBUG_HOOK_FILE
  printf("%s %d  used_blocks=%d\n", __func__, __LINE__, myfs_used_blocks());
#endif
}

void
myfs_close(int fd)
{
  myfs_file_states[fd].flags ^= MYFS_FILE_OPENED;
  myfs_sync();
}

void
myfs_umount()
{
  myfs_sync();
}


