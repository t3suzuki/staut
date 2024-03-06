#ifndef __NVME_H__
#define __NVME_H__


int nvme_read_req(int64_t lba, int num_blk, int tid, int len, char *buf);
int nvme_check(int rid);
int nvme_write_req(int64_t lba, int num_blk, int tid, int len, char *buf);

void nvme_init();
//int nvme_init(int did, int uio_index);

#endif
