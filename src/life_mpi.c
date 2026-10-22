#define _POSIX_C_SOURCE 199309L
#include <time.h>
#include <stdio.h>
#include <stdlib.h> 
#include <mpi.h>

static double now_seconds(void){
      struct timespec ts;
      clock_gettime(CLOCK_MONOTONIC, &ts);
      return ts.tv_sec + ts.tv_nsec * 1e-9;
}


static void step(unsigned char *cur, unsigned char *next, int n, int process_local_rows){
    for (int i = 1; i <= process_local_rows; i++){
        int up =  i - 1;
        int down = i + 1;

        for (int j = 0; j < n; j++){
            int left = (j == 0) ? n - 1 : j - 1;
            int right = (j == n-1) ? 0 : j + 1;
            
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
    size_t size = (size_t)n * n;
    int rank;
    int nprocs;

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &nprocs);
    
    int process_local_rows = n/nprocs;
    if (rank == 0){if (n % nprocs != 0){ fprintf(stderr, "rows (%d) are not fairly assignable to processes (%d)\n",n,nprocs); MPI_Finalize(); return 1;}};

    int first_row = rank * process_local_rows;
        
    size_t process_local_cells = (size_t)(process_local_rows + 2) * n;
    unsigned char *cur = malloc(process_local_cells); // current generation buffer
    unsigned char *next = malloc(process_local_cells);
    if (rank == 0) {if (cur == NULL || next == NULL){ fprintf(stderr, "Failed to allocate memory\n"); MPI_Finalize(); return 1;}};

    int seed = 0;
    srand(seed);
    // this does preserve result integrity, but not top optimization
    for (size_t i = 0; i < (size_t)first_row * n; i++){
        rand();                          // discard the values of the previous processes
    }
    for (size_t k = 0; k < (size_t)process_local_rows * n; k++){     
        cur[n + k] = rand() % 100 < 30;  // my own cells, after the halo row
    }

    int up   = (rank == 0) ? nprocs - 1 : rank - 1;
    int down = (rank == nprocs - 1) ? 0 : rank + 1;


    MPI_Barrier(MPI_COMM_WORLD);   // everyone waits here until all arrive
    double start = now_seconds();
    for (int s = 0; s < steps; s++){

        // send my first row to up, receive my bottom halo row from down
        MPI_Sendrecv(&cur[n], n, MPI_UNSIGNED_CHAR, up, 0, // n is the start of local row 1
            &cur[(size_t)(process_local_rows + 1)*n], n,MPI_UNSIGNED_CHAR, down, 0,
            MPI_COMM_WORLD, MPI_STATUS_IGNORE); // process_local_rows + 1 is the bottom halo row


        // send my last row to down, receive my top halo row from up
        MPI_Sendrecv(&cur[process_local_rows*n], n, MPI_UNSIGNED_CHAR, down, 0, // process_local_rows * n is the start of the last local row
        &cur[0], n, // row 0 is the top halo row
        MPI_UNSIGNED_CHAR, up, 0, // process_local_rows + 1 is the bottom halo row
        MPI_COMM_WORLD, MPI_STATUS_IGNORE); 

        step(cur, next, n, process_local_rows);
        unsigned char *temp = cur;
        cur = next;
        next = temp;
    }
    double end = now_seconds();

    long process_local_alive = 0;
    for (size_t k = n; k < (size_t)n * (process_local_rows + 1);k++){
      process_local_alive += cur[k];
    }

    long global_alive = 0;
    MPI_Reduce(&process_local_alive, &global_alive, 1, MPI_LONG,     
        MPI_SUM, 0, MPI_COMM_WORLD);
    if (rank == 0){
        printf("n=%d steps=%d tempo=%.3fs vive=%ld ns_per_cella=%.3f\n",
            n, steps, end - start, global_alive, (end - start) / size / steps * 1e9);
    }
    
    free(cur);
    free(next);

    MPI_Finalize();
    return 0;
}