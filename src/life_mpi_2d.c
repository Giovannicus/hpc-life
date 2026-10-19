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


static void step(unsigned char *cur, unsigned char *next, int process_local_rows, int process_local_cols, int stride){
    for (int i = 1; i <= process_local_rows; i++){
        int up =  i - 1;
        int down = i + 1;

        for (int j = 1; j <= process_local_cols; j++){
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
    unsigned char *cur = malloc(process_local_cells); //cur è un buffer
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
                cur[(i-first_row+1)*stride + (j-first_col+1)] = v; // convertire indici globali i j a locali
            } 
        }
    }


    // int up   = (rank == 0) ? nprocs - 1 : rank - 1;
    // int down = (rank == nprocs - 1) ? 0 : rank + 1;
    int up, down, left, right;
    MPI_Cart_shift(cart_comm, 0, 1, &up, &down);     // dimension 0 = rows
    MPI_Cart_shift(cart_comm, 1, 1, &left, &right);  // dimension 1 = columns

    // creo il tipo "colonna"
    MPI_Datatype col_type;
    MPI_Type_vector(process_local_rows, 1, stride, // prendo, per ognuno dei process_local_rows bloccchi, 1 cella (la prima) per riga a distanza stride, così simulo la presa di una colonna 
        MPI_UNSIGNED_CHAR, &col_type);
    MPI_Type_commit(&col_type);

    MPI_Barrier(MPI_COMM_WORLD);   // everyone waits here until all arrive
    double start = now_seconds();
    for (int s = 0; s < steps; s++){
        // chiamate su colonne prima per evitare angoli vuoti

        //chiamata per dare la prima colonna a left e prendere sull'ultima halo da right
        MPI_Sendrecv(&cur[stride+1], 1, col_type, left, 0, // l'indice stride + 1  è quello della colonna locale 1, passo 1 perche col_type prevede gia stride elementi
            &cur[(size_t)process_local_cols + stride + 1], 1, col_type, right, 0, // stride in &cur[(size_t)process_local_cols + stride + 1] salta la riga di halo
            cart_comm, MPI_STATUS_IGNORE);
        //chiamata per dare l ultima colonna a right e prendere sulla prima halo da left
        MPI_Sendrecv(&cur[(size_t)process_local_cols + stride], 1, col_type, right, 0, // l'indice stride è quello della colonna locale process_local_cols
            &cur[stride], 1, col_type, left, 0,
            cart_comm, MPI_STATUS_IGNORE);

        //chiamata per dare la prima riga ad up e prendere sull'ultima halo da down
        MPI_Sendrecv(&cur[stride], stride, MPI_UNSIGNED_CHAR, up, 0, // l'indice stride è quello della riga locale 1
            &cur[(size_t)(process_local_rows + 1)*stride], stride,MPI_UNSIGNED_CHAR, down, 0,
            cart_comm, MPI_STATUS_IGNORE); // process_local_rows+1 quella da cui deve prendere da sotto
        //chiamata per dare l ultima riga a down e prendere sulla prima halo da up
        MPI_Sendrecv(&cur[(size_t)process_local_rows*stride], stride, MPI_UNSIGNED_CHAR, down, 0, // l'indice process_local_rows*stride è quello dell ultima riga locale 
            &cur[0], stride, // 0 sarebbe la riga in cui deve prendere da sopra 
            MPI_UNSIGNED_CHAR, up, 0, // process_local_rows+1 quella da cui deve prendere da sotto
            cart_comm, MPI_STATUS_IGNORE); 

        step(cur, next, process_local_rows, process_local_cols, stride);
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