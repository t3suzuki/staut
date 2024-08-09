#ifndef __COMMON_H__
#define __COMMON_H__

//#define N_CORE (1)
#define N_ULT_PER_CORE (512)
#define N_ULT (N_ULT_PER_CORE*N_CORE)

#define BLKSZ (4096)

#define MAX(x,y) ((x > y) ? x : y)
#define MIN(x,y) ((x < y) ? x : y)

#define DIV_CEIL(x,y) ((x+y-1)/y)

extern void (*debug_print)(int, int, int);

#ifndef USE_PTHPTH
#include <abt.h>

#endif

#define INACTIVE_BLOCK (-1)
#define NOT_USED_FD (-1)



#endif 
