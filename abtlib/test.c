#include <time.h>
#include <stdio.h>
#include <stdint.h>
#include <pthread.h>
#include <x86intrin.h>

#define N_TH (16)
#define T_SEC (4)

int begin = 0;
int quit = 0;
uint64_t count[N_TH];

typedef struct {
  int i_th;
} arg_t;

arg_t arg[N_TH];

void
worker(void *p)
{
  arg_t *arg = (arg_t *)p;
  while (!begin) {
    _mm_pause();
  }
  while (!quit) {
    count[arg->i_th]++;
    sched_yield();
  }
}

double c(struct timespec ts)
{
  return ts.tv_sec + ts.tv_nsec * 1e-9;
}

int tmp = 0;
void
worker2()
{
  sleep(3);
  begin = 1;
  printf("begin!\n");
  while (1) {
    sched_yield();
  }
}

int
main()
{
  int i_th;
  struct timespec ts1, ts2;
  pthread_t pth[N_TH];
  pthread_t pth2;
  for (i_th=0; i_th<N_TH; i_th++) {
    arg[i_th].i_th = 0;
    pthread_create(&pth[i_th], NULL, worker, &arg[i_th]);
  }
  printf("%s %d\n", __func__, __LINE__); fflush(0);
  pthread_create(&pth2, NULL, worker2, NULL);

  while (!begin) {
    _mm_pause();
  }
  printf("%s %d\n", __func__, __LINE__);
  clock_gettime(CLOCK_MONOTONIC, &ts1);
  int t;
  for (t=0; ;) {
    clock_gettime(CLOCK_MONOTONIC, &ts2);
    double d = c(ts2) - c(ts1);
    if (d > T_SEC) {
      break;
    }
    if (d > t) {
      t++;
      printf("%d %d\n", t, T_SEC);
    }
    sched_yield();
  }
  clock_gettime(CLOCK_MONOTONIC, &ts2);
  quit = 1;
  for (i_th=0; i_th<N_TH; i_th++) {
    pthread_join(pth[i_th], NULL);
  }
  uint64_t sum = 0;
  for (i_th=0; i_th<N_TH; i_th++) {
    sum += count[i_th];
  }
  double d = c(ts2) - c(ts1);
  printf("%lu M times %f sec\n", sum, d);
  printf("%f ns\n", d / (double)sum * 1000.0 * 1000.0 * 1000.0);
}
