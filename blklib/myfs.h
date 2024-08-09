#ifndef __MYFS_H__
#define __MYFS_H__


void myfs_mount(char *myfs_superblock);
int myfs_open(const char *filename);
int64_t myfs_get_lba(int i, uint64_t offset, int write);
void myfs_umount(void);
void myfs_close(int fd);
void myfs_unlink(const char *filename);
void myfs_unlink_fd(int fd);

#define MYFS_BLOCK_SIZE (2*1024*1024)
#define NUM_BLOCKS (700*1024)

#define MYFS_MAX_BLOCKS_PER_FILE (1024*128)
#define MYFS_MAX_NAMELEN (1024)
#define MYFS_MAX_FILES   (4096)

#define MYFS_MAGIC (0xdeadcafebabefaceULL)

typedef struct {
  uint16_t flags;
  uint16_t ref_count;
} myfs_file_state_t;

typedef struct {
  char name[MYFS_MAX_NAMELEN];
  int32_t block[MYFS_MAX_BLOCKS_PER_FILE];
  uint64_t n_block;
} file_t;

typedef struct {
  uint64_t magic;
  int32_t free_blocks[NUM_BLOCKS];
  int32_t free_blocks_rp;
  int32_t free_blocks_wp;
  file_t file[MYFS_MAX_FILES];
} superblock_t;


extern superblock_t *superblock;
inline uint64_t
myfs_get_size(int i) {
  
  //ult_mutex_lock((ult_mutex *)&myfs_file_mutex[i]);
  uint64_t n_block = superblock->file[i].n_block;
  //ult_mutex_unlock((ult_mutex *)&myfs_file_mutex[i]);
  
  //printf("file %d get n_block %ld\n", i, n_block);
  return n_block * MYFS_BLOCK_SIZE;
}


#define MYFS_FILE_OPENED   (1<<0)
#define MYFS_FILE_UNLINKED (1<<1)

extern myfs_file_state_t myfs_file_states[MYFS_MAX_FILES];

inline int
myfs_increment_ref(int hookfd) {
  if (myfs_file_states[hookfd].flags == MYFS_FILE_OPENED) {
    __sync_fetch_and_add(&myfs_file_states[hookfd].ref_count, 1);
    return 0;
  } else {
    return -1;
  }
}
inline int
myfs_decrement_ref(int hookfd) {
  uint16_t ref_count = __sync_fetch_and_add(&myfs_file_states[hookfd].ref_count, -1);
  if ((ref_count == 0) && (myfs_file_states[hookfd].flags & MYFS_FILE_UNLINKED)) {
    myfs_unlink_fd(hookfd);
  }
}
#endif 
