#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <stdio.h>
#include <sys/mman.h>
#include <unistd.h>
#include <assert.h>
#include <vector>
#include <string>
#include "common.h"
#include "ult.h"
#include <time.h>
#include <zlib.h>

//#define NVME_DEBUG_STAT (1)

//extern void (*debug_print)(long, long, long);

extern "C" {
#include "nvme.h"
  
  //#define ND (1)
#define NVME_NSID (1)
#define NQ (N_CORE+1)
#define QD (N_ULT_PER_CORE*2)
#define AQD (8)

#define RAID_FACTOR (4096 / 512)
#define N_2MB_PAGE MAX(QD * BLKSZ / (2*1024*1024), 1)

#if NVME_DEBUG_STAT
uint64_t debug_stat_req[NQ];
uint64_t debug_stat_rreq[NQ];
uint64_t debug_stat_wreq[NQ];
uint64_t debug_stat_done[NQ];
uint64_t debug_stat_cqdone[NQ];
uint64_t debug_stat_newsqe[NQ];
#endif


static int enable_bus_master(char *pci_addr)
{
  char path[256];
  //sprintf(path, "/sys/class/uio/uio%d/device/config", uio_index);
  sprintf(path, "/sys/bus/pci/devices/%s/config", pci_addr);
  int fd = open(path, O_RDWR);
  uint32_t val;
  int ret = pread(fd, &val, 4, 0x4);
  val = 0x06;
  ret = pwrite(fd, &val, 4, 0x4);
  close(fd);
  return 0;
}

static size_t v2p(size_t vaddr) {
  FILE *pagemap;
  size_t paddr = 0;
  ssize_t offset = (vaddr / sysconf(_SC_PAGESIZE)) * sizeof(uint64_t);
  uint64_t e;

  if ((pagemap = fopen("/proc/self/pagemap", "r"))) {
    if (lseek(fileno(pagemap), offset, SEEK_SET) == offset) {
      if (fread(&e, sizeof(uint64_t), 1, pagemap)) {
	if (e & (1ULL << 63)) {
	  paddr = e & ((1ULL << 55) - 1);
	  paddr = paddr * sysconf(_SC_PAGESIZE);
	  paddr = paddr | (vaddr & (sysconf(_SC_PAGESIZE) - 1));
	}
      }
    }
    fclose(pagemap);
  }
  return paddr;
}


typedef struct {
  struct {
    unsigned OPC : 8;
    unsigned FUSE : 2;
    unsigned Reserved0 : 4;
    unsigned PSDT : 2;
    unsigned CID : 16;
  } CDW0;
  uint32_t NSID;
  uint64_t Reserved0;
  uint64_t MPTR;
  uint64_t PRP1;
  uint64_t PRP2;
  uint32_t CDW10;
  uint32_t CDW11;
  uint32_t CDW12;
  uint32_t CDW13;
  uint32_t CDW14;
  uint32_t CDW15;
} sqe_t;

typedef struct {
  uint32_t DW0;
  uint32_t DW1;
  uint16_t SQHD;
  uint16_t SQID;
  struct {
    unsigned CID : 16;
    unsigned P : 1;
    unsigned SC : 8;
    unsigned SCT : 3;
    unsigned Reserved0 : 2;
    unsigned M : 1;
    unsigned DNR : 1;
  } SF;
} cqe_t;

static char *malloc_2MB()
{
  const int sz = 2*1024*1024;
  char *buf = (char *)mmap(NULL, sz, PROT_READ | PROT_WRITE,
			   MAP_PRIVATE | MAP_HUGETLB | MAP_ANONYMOUS | MAP_POPULATE, 0, 0);
  if (buf == MAP_FAILED) {
    perror("mmap");
    exit(1);
  }
  bzero(buf, sz);
  return buf;
}

class QP {
public:
  int n_sqe;
  int n_cqe;
  const int sq_offset = 0x4000;
#if USE_PREEMPT
  ult_mutex mutex;
#endif
private:
  int sq_tail;
  //int sq_head;
  int cq_head;
  int cq_phase;
  char *sqcq;
  int done_flag[QD];
  volatile uint32_t *doorbell;
  char *buf4k[N_2MB_PAGE];
  uint64_t _buf4k_pa[N_2MB_PAGE];
  int _qid;
  volatile uint32_t *_regs32;
  volatile uint64_t *_regs64;
  
  uint64_t stat_read_count;
  uint64_t stat_read_bytes;
  uint64_t stat_last_read_bytes;
  double stat_read_lasttime;
  uint64_t stat_write_count;
  uint64_t stat_write_bytes;
  uint64_t stat_last_write_bytes;
  double stat_write_lasttime;
public:
  void *rbuf[QD];
  int len[QD];
#if DEBUG
  int64_t lba[QD];
#endif
  inline cqe_t *get_cqe(int index) {
    return (cqe_t *)(sqcq + 0x0) + index;
  }
  inline sqe_t *get_sqe(int index) {
    return (sqe_t *)(sqcq + sq_offset) + index;
  }
  QP(int qid, volatile uint32_t *regs32) {
    _regs32 = regs32;
    _regs64 = (volatile uint64_t*)regs32;
    _qid = qid;
    sq_tail = 0;
    //sq_head = 0;
    cq_head = 0;
    cq_phase = 1;
    n_sqe = (qid == 0) ? AQD : QD;
    n_cqe = (qid == 0) ? AQD : QD;
    sqcq = malloc_2MB();
    //printf("N_2MB_PAGE %d %d\n", N_2MB_PAGE, QD*BLKSZ, QD*BLKSZ/2);
    for (int i=0; i<N_2MB_PAGE; i++) {
      buf4k[i] = malloc_2MB();
      _buf4k_pa[i] = v2p((size_t)buf4k[i]);
    }
    for (int i=0; i<QD; i++) {
      done_flag[i] = 2;
    }
    for (int i=0; i<n_sqe; i++) {
      sqe_t *sqe = get_sqe(i);
      bzero((void*)sqe, sizeof(sqe_t));
      sqe->NSID = NVME_NSID;
      sqe->CDW0.CID = i;
    }
    doorbell = &regs32[0x1000 / sizeof(uint32_t) + 2 * qid];
#if USE_PREEMPT
    ult_mutex_create(&mutex);
#endif

    stat_last_read_bytes = 0;
    stat_read_bytes = 0;
    stat_read_count = 0;
    stat_read_lasttime = 0;
    stat_last_write_bytes = 0;
    stat_write_bytes = 0;
    stat_write_count = 0;
    stat_write_lasttime = 0;
  }
  inline void lock() {
#if USE_PREEMPT
    ult_mutex_lock(&mutex);
#endif
  }
  inline void unlock() {
#if USE_PREEMPT
    ult_mutex_unlock(&mutex);
#endif
  }
  uint64_t cq_pa() {
    return v2p((size_t)sqcq);
  }
  uint64_t sq_pa() {
    return v2p((size_t)sqcq) + sq_offset;
  }
  inline uint64_t buf4k_pa(int cid) {
    int cid_upper = cid % N_2MB_PAGE;
    int cid_lower = cid / N_2MB_PAGE;
    return _buf4k_pa[cid_upper] + BLKSZ * cid_lower;
  }
  inline sqe_t *new_sqe(int *ret_cid = nullptr) {
    sqe_t *sqe = get_sqe(sq_tail);
    int new_sq_tail = (sq_tail + 1) % n_sqe;
    //printf("new_sqe %d %d->%d %d %p\n", __LINE__, sq_tail, new_sq_tail, cq_head, this);
    if (done_flag[sq_tail] == 0) {
      //printf("block %d %d\n", new_sq_tail, cq_head);
      while (done_flag[sq_tail] == 0) {
#if NVME_DEBUG_STAT
	debug_stat_newsqe[_qid]++;
#endif
	check_cq();
      }
    }
    done_flag[sq_tail] = 0;
    if (ret_cid) {
      *ret_cid = sq_tail;
    }
    //printf("new_sqe %d %d->%d %d %p\n", __LINE__, sq_tail, new_sq_tail, cq_head, this);
    sq_tail = new_sq_tail;
    //printf("new_sqe %d %d->%d %d %p\n", __LINE__, sq_tail, new_sq_tail, cq_head, this);
    return sqe;
  }
  inline void sq_doorbell() {
    asm volatile ("" : : : "memory");
    *(doorbell) = sq_tail;
  }
  inline char *get_buf4k(int cid) {
    int cid_upper = cid % N_2MB_PAGE;
    int cid_lower = cid / N_2MB_PAGE;
    return buf4k[cid_upper] + BLKSZ * cid_lower;
  }
  
  inline void increment_read_count(uint64_t len) {
    stat_read_count++;
    stat_read_bytes += len;
    struct timespec tsc;
    //const int unit = 1024*1024;
    const int unit = 128*1024;
    if (stat_read_count % unit == 0) {
      clock_gettime(CLOCK_MONOTONIC, &tsc);
      double cur = tsc.tv_sec + tsc.tv_nsec * 1e-9;
      double delta = cur - stat_read_lasttime;
      uint64_t delta_bytes = stat_read_bytes - stat_last_read_bytes;
      printf("[NVMe STAT] Read qid %d, delta=%f, %f KIOPS, %f Bps\n",
	     _qid, delta, unit/delta/1000, delta_bytes/delta);
      stat_read_lasttime = cur;
      stat_last_read_bytes = stat_read_bytes;
    }
  }

  inline void increment_write_count(uint64_t len) {
    stat_write_count++;
    stat_write_bytes += len;
    struct timespec tsc;
    const int unit = 16*1024;
    if (stat_write_count % unit == 0) {
      clock_gettime(CLOCK_MONOTONIC, &tsc);
      double cur = tsc.tv_sec + tsc.tv_nsec * 1e-9;
      double delta = cur - stat_write_lasttime;
      uint64_t delta_bytes = stat_write_bytes - stat_last_write_bytes;
      printf("[NVMe STAT] Write qid %d, delta=%f, %f KIOPS, %f Bps\n",
	     _qid, delta, unit/delta/1000, delta_bytes/delta);
      stat_write_lasttime = cur;
      stat_last_write_bytes = stat_write_bytes;
    }
  }
  
  void check_cq() {
    /*
    if (cq_head < 0) {
      printf("%d %d %d %p\n", cq_head, sq_tail, cq_phase, sqcq);
      assert(0);
    }
    */
    volatile cqe_t *cqe = get_cqe(cq_head);
    if (cqe->SF.P == cq_phase) {
      do {
	int cid = cqe->SF.CID;
	if (0) {
	  printf("cmd done cid = %d sct=%d sc=%x flag %d\n", cid, cqe->SF.SCT, cqe->SF.SC, cqe->SF.P);
	  printf("buf4k=%p len=%d outbuf=%p\n", get_buf4k(cid), len[cid], rbuf[cid]);
	  /*
	  {
	    unsigned char *buf = (unsigned char *)get_buf4k(cid);
	    printf("buf = %p\n", buf);
	    int i;
	    for (i=0; i<512; i++) {
	      printf("%02x ", buf[i]);
	      if (i % 16 == 15)
		printf("\n");
	    }
	    printf("\n");
	  }
	  */
	}
	/*
	int tmp = cqe->SF.CID >> 8;
	printf("cmd done sqhd=%d %d\n",  cqe->SQHD, cqe->SQID);
	printf("buf4k(cid)[0]=%d\n", ((unsigned char *)get_buf4k(cid))[0]);
	printf("lba lower 8-bits %d\n", tmp);
	*/
	//sq_head = cqe->SQHD;
	if (rbuf[cid]) {
	  memcpy(rbuf[cid], get_buf4k(cid), len[cid]);
	  /*
	  uint32_t checksum = crc32(0x80000000, (const unsigned char *)rbuf[cid], len[cid]);
	  printf("cid=%d checksum=%08x len=%d lba=%ld\n", cid, checksum, len[cid], lba[cid]);
	  */
	  //increment_read_count(len[cid]); // Enabling this line may cause stop here. (including while-loop in new_sqe).
	  /* {
	    unsigned char *buf = (unsigned char *)rbuf[cid];
	    printf("buf = %p\n", buf);
	    int i;
	    for (i=0; i<512; i++) {
	      printf("%02x ", buf[i]);
	      if (i % 16 == 15)
		printf("\n");
	    }
	    printf("\n");
	    } */
	} else {
	  //increment_write_count(len[cid]);
	}
	  
#if NVME_DEBUG_STAT
	debug_stat_cqdone[_qid]++;
#endif
	done_flag[cid] = 1;
	cq_head++;
	if (cq_head == n_cqe) {
	  cq_head = 0;
	  cq_phase ^= 1;
	}
	cqe = get_cqe(cq_head);
      } while (cqe->SF.P == cq_phase);
      *(doorbell+1) = cq_head;
    }
  }
  void req_and_wait(int cid) {
    sq_doorbell();
    while (1) {
      check_cq();
      if (done(cid))
	break;
      //sleep(1);
    }
  }
  inline int done(int cid) {
    return (done_flag[cid] == 1);
  }
};


static QP *qps[ND][NQ]; // +1 is for admin queue.


static void
create_qp(int did, int new_qid)
{
  // CQ create
  {
    int cid;
    volatile sqe_t *sqe = qps[did][0]->new_sqe(&cid);
    sqe->CDW0.OPC = 0x5; // create CQ
    sqe->PRP1 = qps[did][new_qid]->cq_pa();
    sqe->NSID = 0;
    sqe->CDW10 = ((qps[did][new_qid]->n_cqe - 1) << 16) | new_qid;
    sqe->CDW11 = 1;
    //printf("%p %d %p %x\n", sqe, cid, sqe->PRP1, sqe->CDW10);
    qps[did][0]->req_and_wait(cid);
  }
  // SQ create
  {
    int cid;
    volatile sqe_t *sqe = qps[did][0]->new_sqe(&cid);
    sqe->CDW0.OPC = 0x1; // create SQ
    sqe->PRP1 = qps[did][new_qid]->sq_pa();
    sqe->NSID = 0;
    sqe->CDW10 = ((qps[did][new_qid]->n_sqe - 1) << 16) | new_qid;
    sqe->CDW11 = (new_qid << 16) | 1; // physically contiguous
    qps[did][0]->req_and_wait(cid);
  }
  printf("CQ/SQ create done %d\n", new_qid);
}

int
__nvme_init(int did, char *pci_addr)
{
  volatile uint32_t *regs32;
  volatile uint64_t *regs64;
  const int wait_us = 100000;

  enable_bus_master(pci_addr);
  
  char path[256];
  //sprintf(path, "/sys/class/uio/uio%d/device/resource0", uio_index);
  sprintf(path, "/sys/bus/pci/devices/%s/resource0", pci_addr);
  printf("%s\n", path);
  int fd = open(path, O_RDWR);
  if (fd < 0) {
    perror("open");
    return -1;
  }
  regs32 = (volatile uint32_t *)mmap(0, 0x4000, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  regs64 = (volatile uint64_t *)regs32;
  if (regs32 == MAP_FAILED) {
    perror("mmap");
    return -1;
  }

  uint32_t csts;
  uint32_t cc;
  //csts = regs32[0x1c / sizeof(uint32_t)];
  //printf("csts = %u\n", csts);

  /*
  cc = 0x2 << 14;
  regs32[0x14 / sizeof(uint32_t)] = cc; // cc shutdown
  sleep(4);
  csts = regs32[0x1c / sizeof(uint32_t)];
  printf("%u\n", csts);
  */
  
  cc = 0;
  regs32[0x14 / sizeof(uint32_t)] = cc; // cc disable
  usleep(wait_us);
  csts = regs32[0x1c / sizeof(uint32_t)]; // check csts
  printf("csts = %u\n", csts);
  
  assert(csts == 0);

  QP *adq = new QP(0, regs32);
  qps[did][0] = adq;  
  regs64[0x30 / sizeof(uint64_t)] = adq->cq_pa(); // Admin CQ phyaddr
  regs64[0x28 / sizeof(uint64_t)] = adq->sq_pa(); // Admin SQ phyaddr
  regs32[0x24 / sizeof(uint32_t)] = ((adq->n_cqe - 1) << 16) | (adq->n_sqe - 1); // Admin Queue Entry Num
  //printf("%p %lx %p %lx\n", adq->get_cqe(0), adq->cq_pa(), adq->get_sqe(0), adq->sq_pa());

  // enable controller.
  cc = 0x460001;
  regs32[0x14 / sizeof(uint32_t)] = cc; // cc enable
  do {
    usleep(wait_us);
    csts = regs32[0x1c / sizeof(uint32_t)]; // check csts
    //printf("csts = %u\n", csts);
  } while (csts != 1);
  //assert(csts == 1);


  // identity
  {
    printf("identity cmd...\n");
    int cid;
    volatile sqe_t *sqe = adq->new_sqe(&cid);
    sqe->CDW0.OPC = 0x6; // identity
    sqe->NSID = 0xffffffff;
    sqe->CDW10 = 0x1;
    sqe->PRP1 = adq->buf4k_pa(cid);
    adq->req_and_wait(cid);
  }

  {
    int iq;
    for (iq=1; iq<NQ; iq++) {
      qps[did][iq] = new QP(iq, regs32);
      create_qp(did, iq);
      //printf("%p %lx %p %lx\n", qps[iq]->get_cqe(0), qps[iq]->cq_pa(), qps[iq]->get_sqe(0), qps[iq]->sq_pa());
    }
  }


  return 0;
}

void
nvme_init()
{
  char *s = getenv("DRIVE_IDS");
  assert(s);
  printf("ND=%d drive_ids : %s\n", ND, s);
  int i = 0;
  int j = 0;
  while (s[i] != '\0') {
    char pci_addr[13];
    //printf("nvme_init %d %d\n", j, uio_id);
    memcpy(pci_addr, &s[i], 12);

    char path[256];
    sprintf(path, "/sys/bus/pci/drivers/uio_pci_generic/%s/", pci_addr);
    int r = open(path, O_RDONLY);
    if (r < 0) {
      printf("skipping %s. It is not uio.\n", pci_addr);
      exit(1);
    } else {
      printf("nvme_init %d %s\n", j, pci_addr);
      __nvme_init(j++, pci_addr);
    }
    
    if (j == ND)
      break;
    while (s[i] != '_' && s[i] != '\0') {
      i++;
    }
    while (s[i] == '_' && s[i] != '\0') {
      i++;
    }
  }
}


int
nvme_read_req(int64_t lba, int num_blk, int core_id, int len, char *buf)
{
  
  int cid;
  int did = (lba / RAID_FACTOR) % ND;
  int qid = core_id + 1;


  QP *qp = qps[did][qid];

  qp->lock();
#if NVME_DEBUG_STAT
  debug_stat_req[qid] ++;
#endif
  sqe_t *sqe = qp->new_sqe(&cid);
#if NVME_DEBUG_STAT
  debug_stat_rreq[qid] ++;
#endif
  //bzero(sqe, sizeof(sqe_t));
  sqe->PRP1 = qp->buf4k_pa(cid);
  sqe->CDW0.OPC = 0x2; // read
  //sqe->NSID = 1;
  sqe->CDW10 = ((lba / RAID_FACTOR) / ND * RAID_FACTOR) + (lba % RAID_FACTOR);
  sqe->CDW12 = num_blk - 1;
  //sqe->CDW0.CID = ((lba & 0xff) << 8) | cid;
  
  //printf("read_req qid=%d cid=%d lba=%d buf4k=%p buf4k[0]=%02x\n", qid, cid, lba, qps[qid]->get_buf4k(cid), qps[qid]->get_buf4k(cid)[0]);
  qp->rbuf[cid] = buf;
  qp->len[cid] = len;
#if DEBUG
  qp->lba[cid] = lba;
  #endif
  qp->sq_doorbell();
  qp->unlock();
  
  int rid = did*QD*N_CORE + core_id*QD + cid;
  return rid;
}

int
nvme_check(int rid)
{
  int did = rid / QD / N_CORE;
  int qid = (rid / QD) % N_CORE + 1;
  int cid = rid % QD;
  //printf("%s %d rid=%d did=%d qid=%d\n", __func__, __LINE__, rid, did, qid);
  QP *qp = qps[did][qid];
  unsigned char c = qp->get_buf4k(cid)[0];

  if (qp->done(cid)) {
#if NVME_DEBUG_STAT
    debug_stat_done[qid] ++;
#endif
    return 1;
  }
  
  qp->lock();
  qp->check_cq();
  qp->unlock();
  
  if (qp->done(cid)) {
#if NVME_DEBUG_STAT
    debug_stat_done[qid] ++;
#endif
    return 1;
  }
  return 0;
}

int
nvme_write_req(int64_t lba, int num_blk, int core_id, int len, char *buf)
{
  int cid;
  int did = (lba / RAID_FACTOR) % ND;
  int qid = core_id + 1;
  QP *qp = qps[did][qid];

#if NVME_DEBUG_STAT
  debug_stat_req[qid] ++;
#endif

  qp->lock();
  sqe_t *sqe = qp->new_sqe(&cid);
  memcpy(qp->get_buf4k(cid), buf, len);
  sqe->PRP1 = qp->buf4k_pa(cid);
  sqe->CDW0.OPC = 0x1; // write
  sqe->CDW10 = ((lba / RAID_FACTOR) / ND * RAID_FACTOR) + (lba % RAID_FACTOR);
  sqe->CDW12 = num_blk - 1;
  //printf("%s %d lba=%d num_blk=%d qid=%d len=%d cid=%d\n", __func__, __LINE__, lba, num_blk, qid, len, cid);
  qp->rbuf[cid] = NULL;
  qp->len[cid] = 0;
  //qp->len[cid] = len;
  qp->sq_doorbell();
  qp->unlock();
  
  int rid = did*QD*N_CORE + core_id*QD + cid;

  /*
  int o = 0;
  while (o < len) {
    uint32_t checksum = crc32(0x80000000, (const unsigned char *)buf + o, 4096);
    o += 4096;
    printf("[NVMe write req] offset=%d len=4096, checksum=%08x lba=%ld\n", o, checksum, lba);
  }
  */
  //printf("%s rid = %d lba = %d buf=%p len=%d\n", __func__, rid, lba, buf, len);
#if NVME_DEBUG_STAT
  debug_stat_wreq[qid] ++;
#endif
  return rid;
}

#if NVME_DEBUG_STAT
  void nvme_debug_stat()
  {
    int qid, did;
    printf("%s\n", __func__);
    for (qid=0; qid<NQ; qid++) {
      printf("qid=%d, req=%lu (%d+%d), newsqe_try=%lu, cqdone=%lu, done=%lu\n", qid,
	     debug_stat_req[qid],
	     debug_stat_rreq[qid],
	     debug_stat_wreq[qid],
	     debug_stat_newsqe[qid],
	     debug_stat_cqdone[qid], debug_stat_done[qid]);
      if (debug_stat_req[qid] != debug_stat_cqdone[qid]) {
	for (did=0; did<ND; did++) {
	  QP *qp = qps[did][qid];
	  qp->lock();
	  qp->check_cq();
	  qp->unlock();
	}
      }
    }
  }
#endif

  
} // extern "C"
