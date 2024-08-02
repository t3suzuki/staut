#include <time.h>
#include <stdio.h>
#include <pthread.h>

#define N_TH (4)
#define T_SEC (4)

void
worker()
{
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
    pthread_create(&pth[i_th], NULL, worker, NULL);
  }

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
  for (i_th=0; i_th<N_TH; i_th++) {
    pthread_join(pth[i_th], NULL);
  }

}
