#include <stdlib.h>
#include <pthread.h>
#include <abt.h>
#include <errno.h>
#include <assert.h>
#include "real_pthread.h"
#include "common.h"
#include "ult.h"
#include "myfifo.hpp"
#include <immintrin.h> 


extern "C" {

  void __zpoline_init(void);

int mylib_initialized = 0;

__attribute__((constructor(0xffff))) static void
mylib_init()
{
  
  if (!mylib_initialized) {
    mylib_initialized = 1;
    __zpoline_init();
  }
}

} // extern "C"
