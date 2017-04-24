#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <assert.h>
#include <math.h>
#include <pthread.h>

/* Should edit these based on the matrix element type T, below. */
#define CHUNK 24   /* should satisfy 3*(sizeof(T)*CHUNK)^2 < L1 cache size */
#define BLOCK 144  /* should satisfy (sizeof(T)*BLOCK)^2 approx = L2 cache size,
                    * and must be multiple of chunk size */

#define THREADS 16 /* 16 SMT threads */

/* Can define the element type of the matrix here. */
#define T float
#define T_PRINT(x) (printf("%.02f", (x)))
#define T_EQUAL(t1, t2) ((t1) - (t2) <= 0.01 && (t2) - (t1) <= 0.01)
#define T_GEN() ((T)rand() / (T)RAND_MAX)
//#define T unsigned int
//#define T_PRINT(x) (printf("%d", (x)))
//#define T_EQUAL(t1, t2) ((t1) == (t2))
//#define T_GEN() ((T)rand())

typedef T** matrix;

inline matrix new_matrix(int n) {
  T** result = malloc(n * sizeof(T*));
  for (int i = 0; i < n; i++) {
    result[i] = malloc(n * sizeof(T));
  }
  return result;
}

void free_matrix(matrix M, int n) {
  for (int i = 0; i < n; i++) free(M[i]);
  free(M);
}

bool matrices_equal(matrix A, matrix B, int n) {
  bool good = true;
  for (int i = 0; i < n; i++) {
    for (int j = 0; j < n; j++) {
      if (! T_EQUAL(A[i][j], B[i][j])) {
        T_PRINT(A[i][j]); printf(" != "); T_PRINT(B[i][j]); printf("\n");
        good = false;
        return false;
      }
    }
  }
  return good;
}

void initialize_random(matrix M, int n) {
  for (int i = 0; i < n; i++) {
    for (int j = 0; j < n; j++) {
      M[i][j] = T_GEN();
    }
  }
  return;
}

void print_matrix(matrix M, int n) {
  printf("{ ");
  for (int i = 0; i < n; i++) {
    printf("{");
    for (int j = 0; j < n; j++) {
      T_PRINT(M[i][j]);
      if (j < n-1) printf(", ");
    }
    printf("}\n");
    if (i < n-1) printf(", ");
  }
  printf("}\n");
}

/* for reference */
matrix serial_mmt(matrix A, matrix B, int n) {
  matrix C = new_matrix(n);
  for (int i = 0; i < n; i++) {
    for (int j = 0; j < n; j++) {
      T sum = 0.0;
      for (int k = 0; k < n; k++) {
        sum += A[i][k] * B[k][j];
      }
      C[i][j] = sum;
    }
  }
  return C;
}

inline int min(int a, int b) {
  if (a < b) return a;
  else return b;
}

/* initialize the block (bi,bj) in C with zeroes */
inline void zero_block(matrix C, int bsize, int bi, int bj) {
  for (int i = bi; i < bi + bsize; i++)
    for (int j = bj; j < bj + bsize; j++)
      C[i][j] = 0.0;
}

inline void write_block(matrix A, matrix B, matrix C, int bsize, int bi, int bj, int bk) {
  for (int i = bi; i < bi + bsize; i++) {
    for (int j = bj; j < bj + bsize; j++) {
      T sum = 0.0;
      for (int k = bk; k < bk + bsize; k++) sum += A[i][k] * B[k][j];
      C[i][j] += sum;
    }
  }
}

inline void write_block_chunked(matrix A, matrix B, matrix C, int bsize, int csize, int bi, int bj, int bk) {
  for (int ci = bi; ci < bi + bsize; ci += csize) {
    for (int cj = bj; cj < bj + bsize; cj += csize) {
      for (int ck = bk; ck < bk + bsize; ck += csize) {
        write_block(A, B, C, csize, ci, cj, ck);
      }
    }
  }
}

matrix serial_block_chunked_mmt(matrix A, matrix B, int n, int bsize, int csize) {
  matrix C = new_matrix(n);
  for (int bi = 0; bi < n; bi += bsize) {
    for (int bj = 0; bj < n; bj += bsize) {
      zero_block(C, bsize, bi, bj);
      for (int bk = 0; bk < n; bk += bsize) {
        if (csize == 0) write_block(A, B, C, bsize, bi, bj, bk);
        else write_block_chunked(A, B, C, bsize, csize, bi, bj, bk);
      }
    }
  }
  return C;
}

/* Return A*B, where A and B are n-square matrices.
 * Performance configurations:
 *   maxt = max number of threads to use
 *   bsize = block size
 *   csize = chunk size
 * If csize = 0, then do blocked version (rather than block+chunked). */
matrix parallel_block_chunked_mmt(matrix A, matrix B, int n, int maxt, int bsize, int csize) {
  int m = n / bsize;             /* dimension in blocks */
  int num_blocks = m*m;
  int t = min(maxt, num_blocks); /* t = number of threads to use */
  matrix C = new_matrix(n);
  
  void* write_blocks(void* arg) {
    int thread_id = *((int*) arg);
    for (int block_id = thread_id; block_id < num_blocks; block_id += t) {
      int bi = (block_id % m) * bsize;
      int bj = (block_id / m) * bsize;
      zero_block(C, bsize, bi, bj);
      for (int bk = 0; bk < n; bk += bsize) {
        if (csize == 0) write_block(A, B, C, bsize, bi, bj, bk);
        else write_block_chunked(A, B, C, bsize, csize, bi, bj, bk);
      }
    }
    return NULL;
  }

  pthread_t threads[t];
  int args[t];

  for (int i = t-1; i >= 0; i--) {
    args[i] = i;
    if (i > 0) pthread_create(threads + i, NULL, &write_blocks, args + i);
    else write_blocks(args + 0);
  }
  for (int i = t-1; i >= 1; i--) {
    pthread_join(threads[i], NULL);
  }

  return C;
}

int main(int argc, char** argv) {
  
  int maxt = argc > 1 ? atoi(argv[1]) : THREADS;
  int n = argc > 2 ? atoi(argv[2]) : 2048;
  int bsize = argc > 3 ? atoi(argv[3]) : BLOCK;
  int csize = argc > 4 ? atoi(argv[4]) : CHUNK;

  int r = bsize % csize;
  if (r != 0) {
    // printf("rounding block size down to nearest multiple of the chunk size...\n");
    bsize = bsize - r;
  }

  r = n % bsize;
  if (r != 0) {
    // printf("rounding n down to nearest multiple of the block size...\n");
    n = n - r;
  }

  // long bytes = 2 * (sizeof(T) * sizeof(T) * n * n); 
  // printf("n = %d\n", n);
  // printf("bsize = %d\n", bsize);
  // printf("csize = %d\n", csize);

  matrix A = new_matrix(n); initialize_random(A, n);
  matrix B = new_matrix(n); initialize_random(B, n);
  matrix C = parallel_block_chunked_mmt(A, B, n, maxt, bsize, csize);
  free_matrix(A, n);
  free_matrix(B, n);
  free_matrix(C, n);
  
  return 0;
}
