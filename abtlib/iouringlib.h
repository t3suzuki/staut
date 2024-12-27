#ifndef __IOURINGLIB_H__
#define __IOURINGLIB_H__

#define IOURING_DEPTH (1024)

struct io_sq_ring {
	unsigned *head;
	unsigned *tail;
	unsigned *ring_mask;
	unsigned *ring_entries;
	unsigned *flags;
	unsigned *array;
};

struct io_cq_ring {
	unsigned *head;
	unsigned *tail;
	unsigned *ring_mask;
	unsigned *ring_entries;
	struct io_uring_cqe *cqes;
};

struct submitter {
	int ring_fd;
	int enter_ring_fd;
	int index;
	struct io_sq_ring sq_ring;
	struct io_uring_sqe *sqes;
	struct io_cq_ring cq_ring;
  //int inflight;
  //	int tid;
  //	unsigned long reaps;
  //unsigned long done;
  //unsigned long calls;
  //unsigned long io_errors;
  //volatile int finish;

  //__s32 *fds;
  int polled;

  int done_flag[IOURING_DEPTH];
};

int iouring_setup(struct submitter *s, int polled);
int iouring_read(struct submitter *s, int fd, void *buf, size_t count, size_t off);
int iouring_write(struct submitter *s, int fd, void *buf, size_t count, size_t off);
int iouring_check(struct submitter *s);
int iouring_submit(struct submitter *s, unsigned int to_submit);
int iouring_getevents(struct submitter *s);

#endif
