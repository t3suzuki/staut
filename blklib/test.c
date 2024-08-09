#include <time.h>
#include <stdio.h>
#include <pthread.h>

#define N_TH (4)
#define T_SEC (4)

int begin = 0;
int quit = 0;
int count[N_TH];

typedef struct {
  int i_th;
} arg_t;

arg_t arg[N_TH];

void
worker(void *p)
{
  arg_t *arg = (arg_t *)p;
  while (!begin) {
    sched_yield();
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

int
main()
{
  int i_th;
  struct timespec ts1, ts2;
  pthread_t pth[N_TH];
  for (i_th=0; i_th<N_TH; i_th++) {
    arg[i_th].i_th = 0;
    pthread_create(&pth[i_th], NULL, worker, &arg[i_th]);
  }

  begin = 1;
  clock_gettime(CLOCK_MONOTONIC, &ts1);
  int t;
  for (t=0; t<T_SEC; ) {
    clock_gettime(CLOCK_MONOTONIC, &ts2);
    double d = c(ts2) - c(ts1);
    if (d > t) {
      t++;
      printf("%d %d\n", t, T_SEC);
    }
    sched_yield();
  }
  quit = 1;
  for (i_th=0; i_th<N_TH; i_th++) {
    pthread_join(pth[i_th], NULL);
  }
  int sum = 0;
  for (i_th=0; i_th<N_TH; i_th++) {
    sum += count[i_th];
  }
  printf("%f Mops\n", sum / 1000.0 / 1000.0);
}
