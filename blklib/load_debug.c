#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdlib.h>
#include <stdio.h>




void (*debug_print)(long, long, long) = NULL;
void (*debug_print4)(long, long, long, long, long) = NULL;
int (*debug_printf)(const char *format, ...) = NULL;

void load_debug(void)
{
  void *handle;
  {
    const char *filename;
    filename = getenv("LIBDEBUG");
    if (!filename) {
      fprintf(stderr, "env LIBDEBUG is empty, so skip to load a hook library\n");
      return;
    }
	  
    handle = dlmopen(LM_ID_NEWLM, filename, RTLD_NOW | RTLD_LOCAL);
    if (!handle) {
      fprintf(stderr, "dlmopen failed: %s\n\n", dlerror());
      fprintf(stderr, "NOTE: this may occur when the compilation of your hook function library misses some specifications in LDFLAGS. or if you are using a C++ compiler, dlmopen may fail to find a symbol, and adding 'extern \"C\"' to the definition may resolve the issue.\n");
      exit(1);
    }
  }
  {
    debug_print = dlsym(handle, "__debug_print");
    debug_print4 = dlsym(handle, "__debug_print4");
    debug_printf = dlsym(handle, "__debug_printf");
  }
}


