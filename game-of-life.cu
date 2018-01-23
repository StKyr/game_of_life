#include <stdio.h>   /* for printf */
#include <stdlib.h>  /* for malloc, srand, rand */
#include <time.h>    /* for time */


/* cuda specific imports */
#include <cuda.h>
#include <curand.h>
#include <curand_kernel.h>
#include "device_launch_parameters.h"



#ifdef __unix__  // only unix(POSIX) systems can import those libraries (not: will not work in OS X)

#include <getopt.h>  /* for optopt, getopt, etc */
#include <ctype.h>   /* for usleep */
#include <unistd.h>  /* for opterr */

#else

/**
* Importing libraries for argument parsing wont work.
* USE/CAHNGE THOSE VALUES BEFORE COMPILING INSTEAD
*/

#define COLS 1000
#define ROWS COLS
#define SHOULDPRINT 0
#define BLOKS 50
#define THREADS 200
#define MAXITERATIONS 1000

#endif


#define ALIVE_SYMBOL 'O'
#define DEAD_SYMBOL '.'


struct params {
    int Cols;
    int Rows;
    int max_iterations;               // use -1 for unlimited
    int should_print;
    int alive_probability;           // use -1 for default
    int blocks;                      // use -1 for default
    int threads;                     // use -1 for default
} Params;


void parseCommandLineArguments(int argc, char* argv[]);
void initBoardRand(int* board, int alive_prob);
void printBoard(int* board, int width, FILE* fout);

// Auxiliary functions for locating neighboring cells in a (modular) 2D array represented by an 1D array.
__device__ int mod(int a, int b); // Custom MODULUS function because '%' operator may return negative number
__device__ int cell_right(int myid, int _N, int _M, int n);
__device__ int cell_left(int myid, int _N, int _M, int n);
__device__ int cell_down(int myid, int _N, int _M, int n);
__device__ int cell_up(int myid, int _N, int _M, int n);
__device__ int cell_up_right(int myid, int _N, int _M, int n);
__device__ int cell_down_right(int myid, int _N, int _M, int n);
__device__ int cell_down_left(int myid, int _N, int _M, int n);
__device__ int cell_up_left(int myid, int _N, int _M, int n);

__global__ void playGeneration(int *board, int *sums, int *changeHappened, int width, int size);


int main(int argc, char* argv[]) {

    srand((unsigned)time(NULL));

    parseCommandLineArguments(argc, argv);

    int size = (Params.Rows * Params.Cols) * sizeof(int);

    int *board = (int*)malloc(size);

    int *c_board, *c_sums;

    cudaMalloc((void**)&c_board, size);
    cudaMalloc((void**)&c_sums, size);



    int i, j;
    int stateChanged;

    initBoardRand(board, Params.alive_probability);

    cudaMemcpy(c_board, board, size, cudaMemcpyHostToDevice);

    if (Params.should_print) printBoard(board, Params.Cols, stdout);

    clock_t start_clock = clock();
    for (i = 1; (Params.max_iterations == -1) ? 1 : i < Params.max_iterations; i++) {

        playGeneration <<<Params.blocks, Params.threads >>>(board, c_sums, &stateChanged, Params.Cols, Params.Cols*Params.Rows);

        if (Params.should_print) {

            cudaMemcpy(board, c_board, size, cudaMemcpyDeviceToHost);

            for (j = 0; j<50; j++) printf("\n");
            printBoard(board, Params.Cols, stdout);

#ifdef __unix__
            usleep(333 * 1000);
#endif
        }

        if (!stateChanged) {
            break;
        }
    }

    clock_t stop_clock = clock();

    double elapsed_time = (double)(stop_clock - start_clock) / CLOCKS_PER_SEC;
    printf("ELAPSED TIME : %f \n", elapsed_time);

    free(board);

    cudaFree(c_board);
    cudaFree(c_sums);
}



__global__ void playGeneration(int *board, int *sums, int *changeHappened, int width, int size) {

    int i;

    // -------- Count neighbours and store them in a parallel array ------ //

    int local_size = size / gridDim.x;
    int from = blockIdx.x * blockDim.x + threadIdx.x;
    int to = from + local_size;

    for (i = from; i < to; i++) {

        sums[i] = board[cell_up_left(i, width, width, size)]
                  + board[cell_up(i, width, width, size)]
                  + board[cell_up_right(i, width, width, size)]
                  + board[cell_left(i, width, width, size)]
                  + board[cell_right(i, width, width, size)]
                  + board[cell_down_left(i, width, width, size)]
                  + board[cell_down(i, width, width, size)]
                  + board[cell_down_right(i, width, width, size)];


    }
    // --------------------------------------------------- //

    int was_dead;
    int n_neighbours;
    int change = 0;


    // ------------- Based on the # of neighbours, compute next state for each cell -------- //
    for (i = 0; i < size; i++) {

        was_dead = board[i] == 0;
        n_neighbours = sums[i];


        if (n_neighbours < 2) {
            if (was_dead) {
                change = 0;
            } else {
                change = 1;
                board[i] = 0;
            }
        } else if (n_neighbours == 2) {
            change = 0;
            //board[i] = board[i];

        } else if (n_neighbours == 3) {
            if (was_dead) {
                change = 1;
                board[i] = 1;
            } else {
                change = 0;
            }


        } else if (n_neighbours >= 4 && n_neighbours <= 8) {
            if (was_dead) {
                change = 0;
            } else {
                change = 1;
            }
            board[i] = 0;

        }


        *changeHappened = *changeHappened || change;
    }

}












// -------------- Auxiliary business functions ------------------------//



    void initBoardRand(int* board, int alive_prob) {
        int i;
        for (i = 0; i<Params.Rows*Params.Cols; i++) {

            board[i] = rand() % 2 == 0;

        }
    }

void printBoard(int *board, int width, FILE* fout) {

    int i;
    for (i = 0; i<Params.Rows*Params.Cols; i++) {
        fprintf(fout, "%c ", (board[i] == 1) ? ALIVE_SYMBOL : DEAD_SYMBOL);
        if (i % width == width - 1) {
            fprintf(fout, "\n");
        }

    }
}



void parseCommandLineArguments(int argc, char* argv[]) {


#if __unix__

    int c;
    opterr = 0;

    Params.Cols              = -1;
    Params.Rows              = -1;
    Params.alive_probability = 15;
    Params.max_iterations    = 100;




    static int print_flag = 0;
    static int help_flag = 0;

    static struct option long_options[] =
    {
    {"size",       required_argument, 0,           's'},
    {"alive-prob", required_argument, 0,           'a'},
    {"end",        required_argument, 0,           'e'},
    {"print",      no_argument,       &print_flag, 'p'},
    {"help",       no_argument,       &help_flag,  'h'},
    {"blocks",     required_argument, 0,           'b'},
    {"threads",    required_argument, 0,           't'},
    {0, 0, 0, 0}
    };

    int option_index = 0;

    while ((c = getopt_long(argc, argv, "s:a:e:b:t:ph", long_options, &option_index)) != -1)
    switch (c)
    {
    case 's':
    Params.Cols = atoi(optarg);
    Params.Rows = atoi(optarg);

    break;


    case 'a':
    Params.alive_probability = atoi(optarg);
    break;

    case 'e':
    Params.max_iterations = atoi(optarg);
    break;

    case 'b':
    Params.blocks = atoi(optarg);
    break;

    case 't':
    Params.blocks = atoi(optarg);
    break;

    case 'p':
    break;

    case 'h':
    break;

    case '?':
    if (optopt == 'e' || optopt == 'a' || optopt == 't' || optopt == 's' || optopt == 'b' || optopt == 't')
    fprintf (stderr, "Option -%c requires an argument.\n", optopt);

    else if (isprint (optopt))
    fprintf (stderr, "Unknown option `-%c'.\n", optopt);
    else
    fprintf (stderr, "Unknown option character `\\x%x'.\n", optopt);

    exit(1);
    break;
    default:
    break;
    }


    if (help_flag){
    char helpMessage[] = "Usage: game -s SIZE [OPTION]...\n\nA parallel implementation of Game Of Life using Cuda.\n\n\nMANDATORY OPTIONS:\n\n  -s, --size SIZE         Use board of SIZE rows and SIZE columns.\n\nIn this version SIZE must be devided by the square root of the number of MPI processes.\n\nOPTIONAL OPTIONS:\n\n  -b, --blocks BLOCKS     Execute with BLOCKS Cuda blocks (Use -1 for a predefined number - Default).\n  -t, --threads THR       Execute with THR threads (Use -1 for a predefined number - Default).\n  -a, --alive-prob PRO    Use PRO (in %) as probability of spawning an alive creature at each cell in the initial state. (default 15)\n  -e, --end NGEN          End the game after NGEN generations. (default 100) (Use -1 for infinite)\n  -p, --print             Print each state on screen.\n  -h, --help              Display this message and exit.\n";

    #pragma GCC diagnostic ignored "-Wformat-security"
    printf(helpMessage);
    exit(0);
    }

    if (Params.Rows == -1 || Params.Cols == -1){
    fprintf(stderr, "Option -s is required.\n");
    exit(3);
    }

    if (Params.blocks == -1){
    Params.blocks = 8;
    }

    if (Params.threads == -1){
    Params.threads = 100;
    }

    Params.should_print = print_flag;

#else
    Params.Cols = COLS;
    Params.Rows = ROWS;
    Params.blocks = BLOCKS;
    Params.threads = THREADS;
    Params.max_iterations = MAXITERATIONS;
    Params.should_print = SHOULDPRINT;
    Params.alive_probability = 15;
#endif
}




__device__ int mod(int a, int b) {
    int ret = a%b;
    if (ret<0) {
        ret += b;
    }
    return ret;

}

// Auxiliary functions for locating neighbouring cells
__device__ int cell_right(int myid, int _N, int _M, int n) {
    return mod((myid + 1), _M) + (myid / _M) * _M;
}

__device__ int cell_left(int myid, int _N, int _M, int n) {
    return (myid / _M) * _M + mod((myid - 1), _M);
}

__device__ int cell_down(int myid, int _N, int _M, int n) {
    return mod(myid, _M) + mod((myid / _M + 1), _N) * _M;
}

__device__ int cell_up(int myid, int _N, int _M, int n) {
    return mod(myid, _M) + mod((myid / _M - 1), _N) * _M;
}

__device__ int cell_up_right(int myid, int _N, int _M, int n) {
    return cell_up(cell_right(myid, _N, _M, n), _N, _M, n);
}
__device__ int cell_down_right(int myid, int _N, int _M, int n) {
    return cell_down(cell_right(myid, _N, _M, n), _N, _M, n);
}

__device__ int cell_down_left(int myid, int _N, int _M, int n) {
    return cell_down(cell_left(myid, _N, _M, n), _N, _M, n);
}

__device__ int cell_up_left(int myid, int _N, int _M, int n) {
    return cell_up(cell_left(myid, _N, _M, n), _N, _M, n);
}







