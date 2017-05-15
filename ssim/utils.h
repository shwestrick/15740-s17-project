#include <assert.h>
#include <pthread.h>
#ifndef _SSIM_UTILS_H_
#define _SSIM_UTILS_H_

pthread_mutex_t print_lock = PTHREAD_MUTEX_INITIALIZER;

#define assert_msg(cond, args...) \
{ \
  if (!(cond)) { \
    pthread_mutex_lock(&print_lock); \
    fprintf(stderr, "ASSERT_MSG: "); \
    fprintf(stderr, args); \
    fprintf(stderr, "\n"); \
    fflush(stderr); \
    assert(cond); \
    pthread_mutex_unlock(&print_lock); \
  } \
}

#define panic(args...) \
{ \
  pthread_mutex_lock(&print_lock); \
  fprintf(stderr, "PANIC: "); \
  fprintf(stderr, args); \
  fprintf(stderr, "\n"); \
  fflush(stderr); \
  exit(EXIT_FAILURE); \
  pthread_mutex_unlock(&print_lock); \
}

#define info(args...) \
{ \
  pthread_mutex_lock(&print_lock); \
  fprintf(stdout, "INFO: "); \
  fprintf(stdout, args); \
  fprintf(stdout, "\n"); \
  fflush(stdout); \
  pthread_mutex_unlock(&print_lock); \
}

#endif
