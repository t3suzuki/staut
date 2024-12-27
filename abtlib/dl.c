#define _GNU_SOURCE
#include <link.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include "abt.h"

#define DTV_INIT_NUM (16)

static int
dl_callback(struct dl_phdr_info *info, size_t size, void *data)
{
  my_tls_t *my_tls = (my_tls_t *)data;
    char *type;
    int p_type;

    printf("Name: \"%s\" (%d segments)\n", info->dlpi_name,
           info->dlpi_phnum);

    for (size_t j = 0; j < info->dlpi_phnum; j++) {
        p_type = info->dlpi_phdr[j].p_type;
        if (p_type == PT_TLS) {
            printf("    %2zu: [%14p; memsz:%7jx] flags: %#jx; ", j,
                   (void *) (info->dlpi_addr + info->dlpi_phdr[j].p_vaddr),
                   (uintmax_t) info->dlpi_phdr[j].p_memsz,
                   (uintmax_t) info->dlpi_phdr[j].p_flags);
            printf("id=%lu addr=%p ", info->dlpi_tls_modid, info->dlpi_tls_data);

	    my_tls->dtv[info->dlpi_tls_modid].addr = info->dlpi_tls_data;
	    my_tls->dtv[info->dlpi_tls_modid].size = info->dlpi_phdr[j].p_memsz;

        }
    }

    return 0;
}

#define ALIGN(x, y) (((x+y-1)/y)*y)
#define ALIGN_FLOOR(x, y) ((x/y)*y)

void my_dl(my_tls_t *my_tls) {
  my_tls->dtv = calloc(DTV_INIT_NUM, sizeof(dtv_t));
  my_tls->max_dtv = DTV_INIT_NUM;
  my_tls->max_used_dtv = DTV_INIT_NUM;
  dl_iterate_phdr(dl_callback, my_tls);
  int index = 2;
  while (index < my_tls->max_dtv) {
    if (my_tls->dtv[index].size > 0) {
      int64_t diff = my_tls->dtv[index-1].addr - my_tls->dtv[index].addr;
      size_t expected = ALIGN(my_tls->dtv[index].size, 16);
      if (ALIGN(diff, 16) != expected) {
	printf("%s unexpected diff = %lx %lx\n", __func__, diff, expected);
	break;
      }
    } else {
      break;
    }
    index += 1;
  }
  int last = index - 1;
  //printf("%p %lx\n", my_tls->dtv[last].org_addr, ALIGN_FLOOR((uint64_t)my_tls->dtv[last].org_addr, 16));
  my_tls->org_start = ALIGN_FLOOR((uint64_t)my_tls->dtv[last].addr, 16) ;
  uint64_t tls_org_end = ALIGN((uint64_t)my_tls->dtv[1].addr + my_tls->dtv[1].size, 16);
  my_tls->init_mem_size = tls_org_end - my_tls->org_start;
  //printf("%s tls_addr:0x%lx-0x%lx, tls_init_mem_size:0x%lx\n", __func__, my_tls->org_start, tls_org_end, my_tls->init_mem_size);
  int ret = posix_memalign(&my_tls->init_mem, 16, my_tls->init_mem_size);
  assert(ret == 0);
  //printf("%p %ld\n", my_tls, my_tls->init_mem_size);
  memcpy(my_tls->init_mem, (void *)my_tls->org_start, my_tls->init_mem_size);
}
