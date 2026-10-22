#define _POSIX_C_SOURCE 199309L
#include <time.h>
#include <stdio.h>
#include <stdlib.h> 
#include <mpi.h>

enum { TAG_N, TAG_S, TAG_W, TAG_E, TAG_NW, TAG_NE, TAG_SW, TAG_SE };

static double now_seconds(void){
      struct timespec ts;
      clock_gettime(CLOCK_MONOTONIC, &ts);
      return ts.tv_sec + ts.tv_nsec * 1e-9;
}


static void step_region(unsigned char *cur, unsigned char *next,  int row_from, int row_to, int col_from, int col_to, int stride){
    for (int i = row_from; i <= row_to; i++){
        int up =  i - 1;
        int down = i + 1;

        for (int j = col_from; j <= col_to; j++){
            int left = j - 1;
            int right = j + 1;
            
            size_t alive = cur[up * stride + left] + cur[up * stride + j] + cur[up * stride + right] +
                                  cur[i * stride + left] + cur[i * stride + right] +
                                  cur[down * stride + left] + cur[down * stride + j] + cur[down * stride + right];
            if (alive == 3){
                next[i * stride + j] = 1;
            } else if (alive == 2){
                next[i * stride + j] = cur[i * stride + j];
            } else {
                next[i * stride + j] = 0;
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
    int dims[2] = {0,0}; //initialize
    int periods[2] = {1, 1};   // 1 = periodic (toroidal)
    int coords[2];
    MPI_Comm cart_comm;

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &nprocs);

    MPI_Dims_create(nprocs,2,dims);
    MPI_Cart_create(MPI_COMM_WORLD,2,dims,periods,0,&cart_comm);
    MPI_Comm_rank(cart_comm, &rank);
    MPI_Cart_coords(cart_comm, rank, 2, coords);

    int process_local_rows = n/dims[0];
    int process_local_cols = n/dims[1];
    if (rank == 0){if ((n % dims[0] != 0) || (n % dims[1] != 0)) { fprintf(stderr, "rows (%d) are not fairly assignable to processes (%d)\n",n,nprocs); MPI_Finalize(); return 1;}};

    int first_row = coords[0] * process_local_rows;
    int first_col = coords[1] * process_local_cols;
    int stride = process_local_cols + 2;
        
    size_t process_local_cells = (size_t)(process_local_rows + 2) * (process_local_cols + 2);
    unsigned char *cur = malloc(process_local_cells); // current generation buffer
    unsigned char *next = malloc(process_local_cells);
    if (rank == 0) {if (cur == NULL || next == NULL){ fprintf(stderr, "Failed to allocate memory\n"); MPI_Finalize(); return 1;}};

    int seed = 0;
    srand(seed);
    
    int i,j;
    unsigned char v;
    for (i = 0; i < n; i++){
        for (j = 0; j < n; j++){
            v = rand() % 100 < 30;
            if ((i >= first_row && i < first_row + process_local_rows) && (j >= first_col && j < first_col + process_local_cols)){
                cur[(i-first_row+1)*stride + (j-first_col+1)] = v; // convert global indices i, j to local ones
            } 
        }
    }


    // int up   = (rank == 0) ? nprocs - 1 : rank - 1;
    // int down = (rank == nprocs - 1) ? 0 : rank + 1;
    int up, down, left, right;
    MPI_Cart_shift(cart_comm, 0, 1, &up, &down);     // dimension 0 = rows
    MPI_Cart_shift(cart_comm, 1, 1, &left, &right);  // dimension 1 = columns

    int up_left, up_right, down_left, down_right;
    int nb[2];
    nb[0] = coords[0] - 1;  nb[1] = coords[1] - 1;
    MPI_Cart_rank(cart_comm, nb, &up_left);

    nb[0] = coords[0] - 1;  nb[1] = coords[1] + 1;
    MPI_Cart_rank(cart_comm, nb, &up_right);

    nb[0] = coords[0] + 1;  nb[1] = coords[1] - 1;
    MPI_Cart_rank(cart_comm, nb, &down_left);

    nb[0] = coords[0] + 1;  nb[1] = coords[1] + 1;
    MPI_Cart_rank(cart_comm, nb, &down_right);
    // printf("Rank %d coords (%d,%d) | up_left %d up_right %d down_left %d down_right %d\n", rank, coords[0], coords[1], up_left, up_right, down_left, down_right);

    // derived datatype describing one column of the local block
    MPI_Datatype col_type;
    MPI_Type_vector(process_local_rows, 1, stride, // process_local_rows blocks of 1 cell, stride cells apart: one column
        MPI_UNSIGNED_CHAR, &col_type);
    MPI_Type_commit(&col_type);

    MPI_Barrier(MPI_COMM_WORLD);   // everyone waits here until all arrive
    MPI_Request reqs[16];

    double start = now_seconds();
    for (int s = 0; s < steps; s++){
        // tag = direction in which the message travels
        // receives are reqs[0..7], sends are reqs[8..15] (same order as the table)

        // 1. rows: my top row goes up, my bottom halo row comes from down
        MPI_Irecv(&cur[(size_t)(process_local_rows + 1) * stride + 1], process_local_cols, MPI_UNSIGNED_CHAR, down, TAG_N,
            cart_comm, &reqs[0]);
        MPI_Isend(&cur[stride + 1], process_local_cols, MPI_UNSIGNED_CHAR, up, TAG_N,
            cart_comm, &reqs[8]);

        // 2. rows: my bottom row goes down, my top halo row comes from up
        MPI_Irecv(&cur[1], process_local_cols, MPI_UNSIGNED_CHAR, up, TAG_S,
            cart_comm, &reqs[1]);
        MPI_Isend(&cur[(size_t)process_local_rows * stride + 1], process_local_cols, MPI_UNSIGNED_CHAR, down, TAG_S,
            cart_comm, &reqs[9]);

        // 3. columns: my first column goes left, my right halo column comes from right
        MPI_Irecv(&cur[(size_t)stride + process_local_cols + 1], 1, col_type, right, TAG_W,
            cart_comm, &reqs[2]);
        MPI_Isend(&cur[stride + 1], 1, col_type, left, TAG_W,
            cart_comm, &reqs[10]);

        // 4. columns: my last column goes right, my left halo column comes from left
        MPI_Irecv(&cur[stride], 1, col_type, left, TAG_E,
            cart_comm, &reqs[3]);
        MPI_Isend(&cur[(size_t)stride + process_local_cols], 1, col_type, right, TAG_E,
            cart_comm, &reqs[11]);

        // 5. corner: my top-left cell goes to up_left, bottom-right halo corner comes from down_right
        MPI_Irecv(&cur[(size_t)(process_local_rows + 1) * stride + process_local_cols + 1], 1, MPI_UNSIGNED_CHAR, down_right, TAG_NW,
            cart_comm, &reqs[4]);
        MPI_Isend(&cur[stride + 1], 1, MPI_UNSIGNED_CHAR, up_left, TAG_NW,
            cart_comm, &reqs[12]);

        // 6. corner: my top-right cell goes to up_right, bottom-left halo corner comes from down_left
        MPI_Irecv(&cur[(size_t)(process_local_rows + 1) * stride], 1, MPI_UNSIGNED_CHAR, down_left, TAG_NE,
            cart_comm, &reqs[5]);
        MPI_Isend(&cur[(size_t)stride + process_local_cols], 1, MPI_UNSIGNED_CHAR, up_right, TAG_NE,
            cart_comm, &reqs[13]);

        // 7. corner: my bottom-left cell goes to down_left, top-right halo corner comes from up_right
        MPI_Irecv(&cur[process_local_cols + 1], 1, MPI_UNSIGNED_CHAR, up_right, TAG_SW,
            cart_comm, &reqs[6]);
        MPI_Isend(&cur[(size_t)process_local_rows * stride + 1], 1, MPI_UNSIGNED_CHAR, down_left, TAG_SW,
            cart_comm, &reqs[14]);

        // 8. corner: my bottom-right cell goes to down_right, top-left halo corner comes from up_left
        MPI_Irecv(&cur[0], 1, MPI_UNSIGNED_CHAR, up_left, TAG_SE,
            cart_comm, &reqs[7]);
        MPI_Isend(&cur[(size_t)process_local_rows * stride + process_local_cols], 1, MPI_UNSIGNED_CHAR, down_right, TAG_SE,
            cart_comm, &reqs[15]);

        // compute the interior while the halo messages are in flight
        step_region(cur, next, 2, process_local_rows-1, 2, process_local_cols-1, stride);
        MPI_Waitall(16, reqs, MPI_STATUSES_IGNORE);
        step_region(cur, next, 1, 1, 1, process_local_cols, stride);
        step_region(cur, next, process_local_rows, process_local_rows, 1, process_local_cols, stride);
        step_region(cur, next, 2, process_local_rows-1, 1, 1, stride);
        step_region(cur, next, 2, process_local_rows-1, process_local_cols, process_local_cols, stride);
        unsigned char *temp = cur;
        cur = next;
        next = temp;
    }
    double end = now_seconds();
    MPI_Type_free(&col_type);

    long process_local_alive = 0;
    for (int i = 1; i <= process_local_rows; i++){
        for (int j = 1; j <= process_local_cols; j++){
            process_local_alive += cur[i * stride + j];
    }}
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