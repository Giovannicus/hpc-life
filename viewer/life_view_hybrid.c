// Game of Life, hybrid MPI + OpenMP, shown in a window.
// The grid is split in 2D blocks (one per MPI process, like life_hybrid.c).
// Only rank 0 owns the window: every frame it gathers the blocks of all the
// processes into one n x n grid and draws it.
//
// usage: mpirun -np P ./life_view_hybrid [n] [steps] [cell_px] [delay_ms] [hold]
// hold = 0: exit when the steps are over (default 1: keep the last image until ESC)
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mpi.h>
#include <omp.h>
#include "viewer.h"

// one Life step on the local block (halo already filled); OpenMP splits the rows
static void step(unsigned char *cur, unsigned char *next, int rows, int cols, int stride){
    #pragma omp parallel for
    for (int i = 1; i <= rows; i++){
        int up = i - 1;
        int down = i + 1;

        for (int j = 1; j <= cols; j++){
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

// fill the halo of the local block with the cells of the 4 neighbors
// (columns first, then full rows: the corners travel inside the rows)
static void halo_exchange(unsigned char *cur, int rows, int cols, int stride, MPI_Datatype col_type,
                          int up, int down, int left, int right, MPI_Comm comm){
    // my first column goes left, my right halo column comes from right
    MPI_Sendrecv(&cur[stride + 1], 1, col_type, left, 0,
                 &cur[(size_t)stride + cols + 1], 1, col_type, right, 0, comm, MPI_STATUS_IGNORE);
    // my last column goes right, my left halo column comes from left
    MPI_Sendrecv(&cur[(size_t)stride + cols], 1, col_type, right, 0,
                 &cur[stride], 1, col_type, left, 0, comm, MPI_STATUS_IGNORE);
    // my first row goes up, my bottom halo row comes from down
    MPI_Sendrecv(&cur[stride], stride, MPI_UNSIGNED_CHAR, up, 0,
                 &cur[(size_t)(rows + 1) * stride], stride, MPI_UNSIGNED_CHAR, down, 0, comm, MPI_STATUS_IGNORE);
    // my last row goes down, my top halo row comes from up
    MPI_Sendrecv(&cur[(size_t)rows * stride], stride, MPI_UNSIGNED_CHAR, down, 0,
                 &cur[0], stride, MPI_UNSIGNED_CHAR, up, 0, comm, MPI_STATUS_IGNORE);
}

// copy the cells of the block (no halo) into a contiguous buffer of rows * cols bytes
static void pack_block(const unsigned char *cur, unsigned char *buf, int rows, int cols, int stride){
    for (int i = 1; i <= rows; i++){
        memcpy(&buf[(size_t)(i - 1) * cols], &cur[(size_t)i * stride + 1], cols);
    }
}

// rank 0 only: put the block of every process at its place in the n x n grid
static void assemble(const unsigned char *recvbuf, unsigned char *global, int n,
                     int rows, int cols, int nprocs, MPI_Comm comm){
    for (int r = 0; r < nprocs; r++){
        int c[2];
        MPI_Cart_coords(comm, r, 2, c);       // where process r sits in the process grid
        int first_row = c[0] * rows;
        int first_col = c[1] * cols;
        for (int i = 0; i < rows; i++){
            memcpy(&global[(size_t)(first_row + i) * n + first_col],
                   &recvbuf[((size_t)r * rows + i) * cols], cols);
        }
    }
}

int main(int argc, char *argv[])
{
    int n = (argc > 1) ? atoi(argv[1]) : 256;
    int steps = (argc > 2) ? atoi(argv[2]) : 500;
    int cell_px = (argc > 3) ? atoi(argv[3]) : 3;
    int delay_ms = (argc > 4) ? atoi(argv[4]) : 16;
    int hold = (argc > 5) ? atoi(argv[5]) : 1;

    int provided;
    MPI_Init_thread(&argc, &argv, MPI_THREAD_FUNNELED, &provided);

    int rank, nprocs;
    int dims[2] = {0, 0};
    int periods[2] = {1, 1};   // toroidal grid
    int coords[2];
    MPI_Comm cart_comm;
    MPI_Comm_size(MPI_COMM_WORLD, &nprocs);
    MPI_Dims_create(nprocs, 2, dims);
    MPI_Cart_create(MPI_COMM_WORLD, 2, dims, periods, 0, &cart_comm);
    MPI_Comm_rank(cart_comm, &rank);
    MPI_Cart_coords(cart_comm, rank, 2, coords);

    // every process checks the arguments, so that all of them stop together
    if (n < 2 || n > 4096 || n % dims[0] != 0 || n % dims[1] != 0){
        if (rank == 0) fprintf(stderr, "n must be in [2, 4096] and divisible by %d and %d\n", dims[0], dims[1]);
        MPI_Finalize();
        return 1;
    }

    int rows = n / dims[0];
    int cols = n / dims[1];
    int first_row = coords[0] * rows;
    int first_col = coords[1] * cols;
    int stride = cols + 2;

    size_t local_cells = (size_t)(rows + 2) * stride;
    unsigned char *cur = calloc(local_cells, 1);
    unsigned char *next = calloc(local_cells, 1);
    unsigned char *sendbuf = malloc((size_t)rows * cols);
    unsigned char *recvbuf = NULL;   // only rank 0 receives
    unsigned char *global = NULL;    // only rank 0 draws
    if (rank == 0){
        recvbuf = malloc((size_t)nprocs * rows * cols);
        global = calloc((size_t)n * n, 1);
    }
    if (cur == NULL || next == NULL || sendbuf == NULL || (rank == 0 && (recvbuf == NULL || global == NULL))){
        fprintf(stderr, "Failed to allocate memory\n");
        MPI_Abort(MPI_COMM_WORLD, 1);
    }

    // same initial grid as the serial version (same seed, same sequence of rand())
    srand(0);
    for (int i = 0; i < n; i++){
        for (int j = 0; j < n; j++){
            unsigned char value = rand() % 100 < 30;
            if (i >= first_row && i < first_row + rows && j >= first_col && j < first_col + cols){
                cur[(i - first_row + 1) * stride + (j - first_col + 1)] = value;
            }
        }
    }

    int up, down, left, right;
    MPI_Cart_shift(cart_comm, 0, 1, &up, &down);
    MPI_Cart_shift(cart_comm, 1, 1, &left, &right);

    MPI_Datatype col_type;   // one column of the local block
    MPI_Type_vector(rows, 1, stride, MPI_UNSIGNED_CHAR, &col_type);
    MPI_Type_commit(&col_type);

    // only rank 0 opens the window; it tells the others if it worked
    Viewer *v = NULL;
    int ok = 1;
    if (rank == 0){
        v = viewer_open("life hybrid", n, n, cell_px);
        if (v == NULL) ok = 0;
        else viewer_set_delay(v, delay_ms);
    }
    MPI_Bcast(&ok, 1, MPI_INT, 0, cart_comm);
    if (!ok){
        if (rank == 0) fprintf(stderr, "Failed to create viewer\n");
        MPI_Type_free(&col_type);
        free(cur); free(next); free(sendbuf); free(recvbuf); free(global);
        MPI_Finalize();
        return 1;
    }

    double t_compute = 0, t_gather = 0, t_draw = 0;   // time per phase, summed over the frames
    int frames = 0;
    int quit = 0;
    for (int s = 0; s < steps; s++){
        // rank 0 reads the keyboard; everybody must stop at the same frame
        int running = 1;
        if (rank == 0) running = viewer_poll(v);
        MPI_Bcast(&running, 1, MPI_INT, 0, cart_comm);
        if (!running){ quit = 1; break; }

        double t0 = MPI_Wtime();
        halo_exchange(cur, rows, cols, stride, col_type, up, down, left, right, cart_comm);
        step(cur, next, rows, cols, stride);
        unsigned char *temp = cur; cur = next; next = temp;

        double t1 = MPI_Wtime();
        pack_block(cur, sendbuf, rows, cols, stride);
        MPI_Gather(sendbuf, rows * cols, MPI_UNSIGNED_CHAR,
                   recvbuf, rows * cols, MPI_UNSIGNED_CHAR, 0, cart_comm);
        if (rank == 0) assemble(recvbuf, global, n, rows, cols, nprocs, cart_comm);

        double t2 = MPI_Wtime();
        if (rank == 0) viewer_draw(v, global);

        double t3 = MPI_Wtime();
        t_compute += t1 - t0;
        t_gather += t2 - t1;
        t_draw += t3 - t2;
        frames++;
    }

    // steps are over: keep the last image until the user quits
    while (hold && !quit){
        int running = 1;
        if (rank == 0){
            running = viewer_poll(v);
            viewer_draw(v, global);
        }
        MPI_Bcast(&running, 1, MPI_INT, 0, cart_comm);
        if (!running) quit = 1;
    }

    if (rank == 0 && frames > 0){
        long alive = 0;   // cells alive in the last gathered grid: compare with the serial reference
        for (size_t k = 0; k < (size_t)n * n; k++) alive += global[k];
        printf("n=%d procs=%d threads=%d frames=%d alive=%ld | per frame (ms): compute=%.3f gather=%.3f draw=%.3f (draw includes delay=%d)\n",
               n, nprocs, omp_get_max_threads(), frames, alive,
               1e3 * t_compute / frames, 1e3 * t_gather / frames, 1e3 * t_draw / frames, delay_ms);
    }

    if (rank == 0) viewer_close(v);
    MPI_Type_free(&col_type);
    free(cur); free(next); free(sendbuf); free(recvbuf); free(global);
    MPI_Finalize();
    return 0;
}
