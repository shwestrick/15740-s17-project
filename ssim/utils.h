#include <assert.h>
#ifndef _SSIM_UTILS_H_
#define _SSIM_UTILS_H_

#define panic(args...) \
{ \
  fprintf(stderr, "PANIC: "); \
  fprintf(stderr, args); \
  fprintf(stderr, "\n"); \
  fflush(stderr); \
  exit(EXIT_FAILURE); \
}

#define info(args...) \
{ \
  fprintf(stdout, "INFO: "); \
  fprintf(stdout, args); \
  fprintf(stdout, "\n"); \
  fflush(stdout); \
}

#endif
