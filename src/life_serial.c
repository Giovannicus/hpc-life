#define _POSIX_C_SOURCE 199309L
#include <time.h>
#include <stdio.h>
#include <stdlib.h> 

static double now_seconds(void){
      struct timespec ts;
      clock_gettime(CLOCK_MONOTONIC, &ts);
      return ts.tv_sec + ts.tv_nsec * 1e-9;
}


static void step(unsigned char *cur, unsigned char *next, int n){
    for (int i = 0; i < n; i++){
        int up = (i - 1 + n) % n;
        int down = (i + 1) % n;

        for (int j = 0; j < n; j++){
            int left = (j - 1 + n) % n;
            int right = (j + 1) % n;
            
            size_t alive = cur[up * n + left] + cur[up * n + j] + cur[up * n + right] +
                                  cur[i * n + left] + cur[i * n + right] +
                                  cur[down * n + left] + cur[down * n + j] + cur[down * n + right];
            if (alive == 3){
                next[i * n + j] = 1;
            } else if (alive == 2){
                next[i * n + j] = cur[i * n + j];
            } else {
                next[i * n + j] = 0;
            }
        }
    }
}

int main(int argc, char *argv[])
{
    int n = (argc > 1) ? atoi(argv[1]) : 1024;
    int steps = (argc > 2) ? atoi(argv[2]) : 100;

    printf("n = %d, steps = %d\n", n, steps);
    size_t size = (size_t)n * n;
    unsigned char *cur = malloc(size);
    unsigned char *next = malloc(size);
    if (cur == NULL || next == NULL){ fprintf(stderr, "Failed to allocate memory\n"); return 1;}
    
    int seed = 0;
    srand(seed);
    for (size_t i = 0; i < size; i++){
        cur[i] = rand() % 100 < 30;
    }

    double start = now_seconds();
    for (int s = 0; s < steps; s++){
        step(cur, next, n);
        unsigned char *temp = cur;
        cur = next;
        next = temp;
    }
    double end = now_seconds();

    long alive = 0;
    for (size_t k = 0; k < size; k++){
        alive += cur[k];
    }
    printf("Living cells: %ld\n", alive);
    printf("Time taken: %f seconds\n", end - start);
    printf("Time per cell and step: %f nanoseconds\n", (end - start) / size / steps *1e9);
    free(cur);
    free(next);

    return 0;
}