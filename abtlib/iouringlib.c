#include <stdio.h>
#include <sys/syscall.h>
#include <sys/mman.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <stdatomic.h>

#include "io_uring.h"
#include "iouringlib.h"

#define atomic_load_acquire(p)					\
	atomic_load_explicit((_Atomic typeof(*(p)) *)(p),	\
			     memory_order_acquire)
#define atomic_store_release(p, v)				\
	atomic_store_explicit((_Atomic typeof(*(p)) *)(p), (v),	\
			      memory_order_release)

int
iouring_check(struct submitter *s)
{
  struct io_cq_ring *ring = &s->cq_ring;
  struct io_uring_cqe *cqe;
  unsigned head, reaped = 0;
  int last_idx = -1, stat_nr = 0;

  head = *ring->head;
  do {
    if (head == atomic_load_acquire(ring->tail))
      break;
    cqe = &ring->cqes[head & *(ring->ring_mask)];
    if (cqe->res < 0) {
      printf("io: unexpected ret=%d\n", cqe->res);
      if (s->polled && (cqe->res == -EOPNOTSUPP))
	printf("Your filesystem/driver/kernel doesn't support polled IO\n");
      return -1;
    } else {
      printf("%d\n", cqe->res);
    }
    s->done_flag[cqe->user_data] = 1;
    reaped++;
    head++;
  } while (1);

  if (reaped) {
    atomic_store_release(ring->head, head);
  }
  return reaped;
}

int
iouring_get_sqe(struct submitter *s)
{
  struct io_sq_ring *ring = &s->sq_ring;
  unsigned head, index, tail, next_tail;
  
  head = *ring->head;
  next_tail = tail = *ring->tail;
  
  next_tail++;
  if (next_tail == head)
    return -1;

  index = tail & *(s->sq_ring.ring_mask);
    
  tail = next_tail;

  atomic_store_release(ring->tail, tail);

  return index;
}

int
iouring_op(struct submitter *s, int op, int fd, void *buf, size_t count, size_t off)
{
  int index = iouring_get_sqe(s);
  if (index < 0)
    return -1;
  
  struct io_uring_sqe *sqe = &s->sqes[index];
  sqe->flags = 0;
  sqe->fd = fd;
  sqe->opcode = op;
  sqe->addr = (uint64_t)buf;
  sqe->len = count;
  sqe->buf_index = 0;
  sqe->ioprio = 0;
  sqe->off = off;
  sqe->user_data = index;
  s->done_flag[index] = 0;
  return index;
}

int
iouring_read(struct submitter *s, int fd, void *buf, size_t count, size_t off)
{
  return iouring_op(s, IORING_OP_READ, fd, buf, count, off);
}

int
iouring_write(struct submitter *s, int fd, void *buf, size_t count, size_t off)
{
  return iouring_op(s, IORING_OP_WRITE, fd, buf, count, off);
}

int iouring_submit(struct submitter *s, unsigned int to_submit)
{
  return syscall(__NR_io_uring_enter, s->enter_ring_fd, to_submit,
		 0, 0, NULL, 0);
}

int iouring_getevents(struct submitter *s)
{
  unsigned flags = IORING_ENTER_GETEVENTS;
  return syscall(__NR_io_uring_enter, s->enter_ring_fd, 0,
		 1, flags, NULL, 0);
}

static int __iouring_setup(unsigned entries, struct io_uring_params *p)
{
  int ret;

  p->flags |= IORING_SETUP_CQSIZE;
  p->cq_entries = entries;

  ret = syscall(__NR_io_uring_setup, entries, p);
  if (!ret)
    return 0;
  
  return ret;
}

int iouring_setup(struct submitter *s, int polled)
{
  struct io_sq_ring *sring = &s->sq_ring;
  struct io_cq_ring *cring = &s->cq_ring;
  struct io_uring_params p;
  int ret, fd, i;
  void *ptr;
  size_t len;
  
  memset(&p, 0, sizeof(p));

  if (polled)
    p.flags |= IORING_SETUP_IOPOLL;

  fd = __iouring_setup(IOURING_DEPTH, &p);
  if (fd < 0) {
    perror("io_uring_setup");
    return 1;
  }
  s->ring_fd = s->enter_ring_fd = fd;
  
  ptr = mmap(0, p.sq_off.array + p.sq_entries * sizeof(__u32),
	     PROT_READ | PROT_WRITE, MAP_SHARED | MAP_POPULATE, fd,
	     IORING_OFF_SQ_RING);
  sring->head = ptr + p.sq_off.head;
  sring->tail = ptr + p.sq_off.tail;
  sring->ring_mask = ptr + p.sq_off.ring_mask;
  sring->ring_entries = ptr + p.sq_off.ring_entries;
  sring->flags = ptr + p.sq_off.flags;
  sring->array = ptr + p.sq_off.array;
  //sq_ring_mask = *sring->ring_mask;
  
  len = p.sq_entries * sizeof(struct io_uring_sqe);
  s->sqes = mmap(0, len,
		 PROT_READ | PROT_WRITE, MAP_SHARED | MAP_POPULATE, fd,
		 IORING_OFF_SQES);
  
  len = p.cq_off.cqes +
    p.cq_entries * sizeof(struct io_uring_cqe);
  
  ptr = mmap(0, len,
	     PROT_READ | PROT_WRITE, MAP_SHARED | MAP_POPULATE, fd,
	     IORING_OFF_CQ_RING);
  cring->head = ptr + p.cq_off.head;
  cring->tail = ptr + p.cq_off.tail;
  cring->ring_mask = ptr + p.cq_off.ring_mask;
  cring->ring_entries = ptr + p.cq_off.ring_entries;
  cring->cqes = ptr + p.cq_off.cqes;
  //cq_ring_mask = *cring->ring_mask;
  
  for (i = 0; i < p.sq_entries; i++)
    sring->array[i] = i;
  
  for (i = 0; i < IOURING_DEPTH; i++)
    s->done_flag[i] = 1;

  s->polled = polled;
  
  return 0;
}
