#include <assert.h>  /* for assert */
#include <math.h>    /* for sqrt */
#include <stdlib.h>  /* for malloc/free */
#include <getopt.h>
#include <unistd.h>
#include <ctype.h>
#include <string.h>

#include "lib.h"

/// deleteme
#include "mpi.h"


int I_AM_MASTER(int myid) {
    return MASTER_PROC_ID == myid;
}

int I_AM_SLAVE(int myid) {
    return MASTER_PROC_ID != myid;
}



void parseCommandLineArguments(int argc, char* argv[]){

    int c;
    opterr = 0; /* defined in unitstd.h */


    /*
     * REQUIRED:
     * -r X: Use table of X rows.
     * -c X: Use table of X columns.
     *
     * OPTIONAL:
     * -t X: Execute with X threads (if possible) (Use -1 for maximum number possible - Default).
     * -a X: Use X (in %) as probability of spawning an alive creature at each cells in the initial state. (default 15)
     * -s X: Stop the game after X generations. (default 100) (Use -1 for infinite)
     * -p  : Print each state on screen.
     * -h  : Display help message.
     */

    Params.Cols              = -1;
    Params.Rows              = -1;
    Params.numthreads        = -1;
    Params.alive_probability = 15;
    Params.max_iterations    = 100;



    static int print_flag = 0;
    static int help_flag = 0;

    static struct option long_options[] =
            {
                    {"size",       required_argument, 0,           's'},
                    {"threads",    required_argument, 0,           't'},
                    {"alive-prob", required_argument, 0,           'a'},
                    {"end",        required_argument, 0,           'e'},
                    {"print",      no_argument,       &print_flag, 'p'},
                    {"help",       no_argument,       &help_flag,  'h'},
                    {0, 0, 0, 0}
            };

    int option_index = 0;

    while ((c = getopt_long(argc, argv, "s:t:a:e:ph", long_options, &option_index)) != -1)
        switch (c)
        {
            case 's':
                Params.Cols = atoi(optarg);
                Params.Rows = atoi(optarg);

                break;

            case 't':
                Params.numthreads = atoi(optarg);
                break;

            case 'a':
                Params.alive_probability = atoi(optarg);
                break;

            case 'e':
                Params.max_iterations = atoi(optarg);
                break;

            case 'p':
                break;

            case 'h':
                break;

            case '?':
                if (optopt == 'e' || optopt == 'a' || optopt == 't' || optopt == 's')
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
        char* helpMessage = "Usage: game -s SIZE [OPTION]...\n\nA parallel implementation of Game Of Life using MPI and OpenMP.\n\n\nMANDATORY OPTIONS:\n\n  -s, --size SIZE         Use board of SIZE rows and SIZE columns.\n\nIn this version SIZE must be devided by the square root of the number of MPI processes.\n\nOPTIONAL OPTIONS:\n\n  -t, --threads THR       Execute with THR threads (if possible) (Use -1 for maximum number possible - Default).\n  -a, --alive-prob PRO    Use PRO (in %) as probability of spawning an alive creature at each cell in the initial state. (default 15)\n  -e, --end NGEN          End the game after NGEN generations. (default 100) (Use -1 for infinite)\n  -p, --print             Print each state on screen.\n  -h, --help              Display this message and exit.\n";

#pragma GCC diagnostic ignored "-Wformat-security"
        printf(helpMessage);
        exit(0);
    }

    if (Params.Rows == -1 || Params.Cols == -1){
        fprintf(stderr, "Option -s is required.\n");
        exit(3);
    }


    Params.should_print = print_flag;

}

// Actual MOD operation. (GCC's "%" doesn't always return positive values.)
int mod(int a, int b){
    int ret=a%b;
    if(ret<0){
        ret+=b;
    }
    return ret;
}

// This is an (emulated) namespace for directions
struct directions{
    int UP,
        DOWN,
        LEFT,
        RIGHT,
        UP_LEFT,
        UP_RIGHT,
        DOWN_LEFT,
        DOWN_RIGHT;
} Direction = {.UP =1000, .DOWN = 1001, .LEFT = 1002, .RIGHT = 1003, .UP_LEFT = 1004, .UP_RIGHT = 1005, .DOWN_LEFT=1006, .DOWN_RIGHT=1007};


// Auxiliary function for locating neighbouring processes
int proc_id_right(int myid, int _N, int _M, int n){
    return mod((myid + 1), _M) + (myid/_M) * _M;
}

int proc_id_left(int myid, int _N, int _M, int n){
    return (myid/_M) * _M + mod((myid - 1), _M);
}

int proc_id_down(int myid, int _N, int _M, int n){
    return mod(myid, _M) + mod((myid / _M + 1), _N) * _M;
}

int proc_id_up(int myid, int _N, int _M, int n){
    return mod(myid, _M) + mod((myid / _M - 1), _N) * _M;
}

int proc_id_up_right(int myid, int _N, int _M, int n){
    return proc_id_up(proc_id_right(myid, _N,_M, n), _N, _M, n);
}

int proc_id_down_right(int myid, int _N, int _M, int n){
    return proc_id_down(proc_id_right(myid, _N,_M, n), _N, _M, n);
}

int proc_id_down_left(int myid, int _N, int _M, int n){
    return proc_id_down(proc_id_left(myid, _N,_M, n), _N, _M, n);
}

int proc_id_up_left(int myid, int _N, int _M, int n){
    return proc_id_up(proc_id_left(myid, _N, _M, n), _N, _M, n);
}


// Auxiliary functions for locating neighbouring cells
int cell_right(int myid, int _M){
    return (myid + 1);
}

int cell_left(int myid, int _M){
    return (myid-1);
}

int cell_down(int myid, int _M){
    return (myid + _M);
}

int cell_up(int myid, int _M){
    return (myid - _M);
}

int cell_up_right(int myid, int _M){
    return (myid-_M+1);
}

int cell_down_right(int myid, int _M){
    return (myid+_M+1);
}

int cell_down_left(int myid, int _M){
    return (myid+_M-1);
}

int cell_up_left(int myid, int _M){
    return (myid-_M-1);
}



// Business Logic Functions
void printState(int numprocs, int width, int *** board){
    int row, col, i, j;
    for(i=0;i<(int) sqrt(numprocs);i++){
        for (row=0; row<width; row++){
            for (j=0; j< (int) sqrt(numprocs); j++){
                for (col=0; col < width; col++){
                    printf("%c ",(board[i][j][row*width+col] == 0) ? EMPTY_SYMBOL : (board[i][j][row*width+col] == 1) ? CREATURE_SYMBOL : '?');
                }
            }
            printf("\n");
        }
    }
}

// TODO: mention probability parameter
void initializeBoard(int *subboard, int dimX, int dimY, int prob){

    float _prob = 1.0f - prob / 100.0f;
    int i;
    for(i=0;i<dimX*dimY;i++){
        subboard[i]=rand()/(float) RAND_MAX > _prob;
    }

}



int isOuter(int i, int width){
    return ((mod(i, width) == 0) || (mod(i, width) == width - 1) || (i < width) || (i >= width * width - width));
}




int countNeighboursInner(int i, const int *temp, int width){

    int sum;

    sum =   temp[cell_up_left(i,width)]
            +temp[cell_up(i,width)]
            +temp[cell_up_right(i,width)]
            +temp[cell_left(i,width)]
            +temp[cell_right(i,width)]
            +temp[cell_down_left(i,width)]
            +temp[cell_down(i,width)]
            +temp[cell_down_right(i,width)];

    return sum;

}



int countNeighboursOuter(int i, const int *temp, int width, int *up_buffer, int *down_buffer, int *left_buffer,
                    int *right_buffer, int up_left_buffer, int up_right_buffer,
                    int down_left_buffer, int down_right_buffer){

    int sum;

    if((i<width) && !(mod(i,width)==0) && !(mod(i,width)==(width-1))){		//up edge
        sum =   up_buffer[i]
                +up_buffer[i-1]
                +up_buffer[i+1]
                +temp[cell_left(i,width)]
                +temp[cell_right(i,width)]
                +temp[cell_down_left(i,width)]
                +temp[cell_down(i,width)]
                +temp[cell_down_right(i,width)];
    }
    else if(!(mod(i,width)==0) && !(mod(i,width)==(width-1)) && (i>=width*width-width)){		//down edge
        sum =   temp[cell_up_left(i,width)]
                +temp[cell_up(i,width)]
                +temp[cell_up_right(i,width)]
                +temp[cell_left(i,width)]
                +temp[cell_right(i,width)]
                +down_buffer[mod(i,width)]
                +down_buffer[mod(i, width)-1]
                +down_buffer[mod(i, width)+1];
    }
    else if((mod(i,width)==0) && !(i<width) && !(i>=width*width-width)){		//left edge
        sum =   temp[cell_up(i,width)]
                +temp[cell_up_right(i,width)]
                +temp[cell_right(i,width)]
                +temp[cell_down(i,width)]
                +temp[cell_down_right(i,width)]
                +left_buffer[i/width-1]
                +left_buffer[i/width]
                +left_buffer[i/width+1];
    }
    else if((mod(i,width)==(width-1)) && !(i<width) && !(i>=width*width-width)){		//right edge
        sum =   temp[cell_up_left(i,width)]
                +temp[cell_up(i,width)]
                +temp[cell_left(i,width)]
                +temp[cell_down_left(i,width)]
                +temp[cell_down(i,width)]
                +right_buffer[i/width-1]
                +right_buffer[i/width]
                +right_buffer[i/width+1];
    }
    else if(i==0){		//up-left corner
        sum =   up_buffer[0]
                +up_buffer[1]
                +left_buffer[0]
                +left_buffer[1]
                +up_left_buffer
                +temp[cell_right(i,width)]
                +temp[cell_down(i,width)]
                +temp[cell_down_right(i,width)];
    }
    else if(i==(width-1)){		//up-right corner
        sum =   temp[cell_left(i,width)]
                +temp[cell_down_left(i,width)]
                +temp[cell_down(i,width)]
                +up_buffer[width-2]
                +up_buffer[width-1]
                +right_buffer[0]
                +right_buffer[1]
                +up_right_buffer;
    }
    else if(i==(width*(width-1))){		//down-left corner
        sum =   temp[cell_up(i,width)]
                +temp[cell_up_right(i,width)]
                +temp[cell_right(i,width)]
                +down_buffer[0]
                +down_buffer[1]
                +left_buffer[width-1]
                +left_buffer[width-2]
                +down_left_buffer;
    }
    else if(i==((width*width)-1)){		//down-right corner
        sum =   temp[cell_up_left(i,width)]
                +temp[cell_up(i,width)]
                +temp[cell_left(i,width)]
                +right_buffer[width-1]
                +right_buffer[width-2]
                +down_buffer[width-1]
                +down_buffer[width-2]
                +down_right_buffer;
    }else{
        assert(1==2); // should never go here
    }

    return sum;
}



int updateLocalState(const int *sums, int *temp, int width){

    int i, flag=0;

#pragma omp for private(i)
    for(i=0;i<width*width;i++){

            if ((sums[i] == 0) || (sums[i] == 1)) {
                if (temp[i] == 1) {
                    temp[i] = 0;
                    flag = 1;
                }
            } else if ((sums[i] == 2) || (sums[i] == 3)) {
                if (sums[i] == 2) {}
                else {
                    if (temp[i] == 0) {
                        temp[i] = 1;
                        flag = 1;
                    }
                }
            } else if ((sums[i] == 4) || (sums[i] == 5) || (sums[i] == 6) || (sums[i] == 7) || (sums[i] == 8)) {
                if (temp[i] == 1) {
                    temp[i] = 0;
                    flag = 1;
                }
            }
    }
    return flag;
}



int checkGlobalStateChanged(int myState){
    int change;
    MPI_Allreduce(&myState, &change, 1, MPI_INT, MPI_LOR, MPI_COMM_WORLD); // OR operation should be faster than sum;
    return change;
}

// TO CHECK: sending to different neighbours uses different tags
void sendPeripheralsToNeighbours(int myid, int *temp, int width, int numprocs,
                                 MPI_Request *sHandlerUp, MPI_Request *sHandlerDown, MPI_Request *sHandlerLeft, MPI_Request *sHandlerRight,
                                 MPI_Request *sHandlerUpLeft, MPI_Request *sHandlerUpRight, MPI_Request *sHandlerDownLeft, MPI_Request *sHandlerDownRight){
    int i,j;

    int id_up         = proc_id_up        (myid,(int)sqrt(numprocs), (int)sqrt(numprocs), numprocs);
    int id_down       = proc_id_down      (myid,(int)sqrt(numprocs), (int)sqrt(numprocs), numprocs);
    int id_right      = proc_id_right     (myid,(int)sqrt(numprocs), (int)sqrt(numprocs), numprocs);
    int id_left       = proc_id_left      (myid,(int)sqrt(numprocs), (int)sqrt(numprocs), numprocs);
    int id_up_left    = proc_id_up_left   (myid,(int)sqrt(numprocs), (int)sqrt(numprocs), numprocs);
    int id_up_right   = proc_id_up_right  (myid,(int)sqrt(numprocs), (int)sqrt(numprocs), numprocs);
    int id_down_left  = proc_id_down_left (myid,(int)sqrt(numprocs), (int)sqrt(numprocs), numprocs);
    int id_down_right = proc_id_down_right(myid,(int)sqrt(numprocs), (int)sqrt(numprocs), numprocs);


    MPI_Datatype columnVector;
    MPI_Type_vector(width,1,width,MPI_INT,&columnVector);
    MPI_Type_commit(&columnVector);

    MPI_Isend(temp,                     width, MPI_INT,      id_up,         Direction.UP,         MPI_COMM_WORLD, &(*sHandlerUp));
    MPI_Isend(&(temp[width*(width-1)]), width, MPI_INT,      id_down,       Direction.DOWN,       MPI_COMM_WORLD, &(*sHandlerDown));
    MPI_Isend(temp,                     1,     columnVector, id_left,       Direction.LEFT,       MPI_COMM_WORLD, &(*sHandlerLeft));
    MPI_Isend(&(temp[width-1]),         1,     columnVector, id_right,      Direction.RIGHT,      MPI_COMM_WORLD, &(*sHandlerRight));
    MPI_Isend(temp,                     1,     MPI_INT,      id_up_left,    Direction.UP_LEFT,    MPI_COMM_WORLD, &(*sHandlerUpLeft));
    MPI_Isend(&(temp[width-1]),         1,     MPI_INT,      id_up_right,   Direction.UP_RIGHT,   MPI_COMM_WORLD, &(*sHandlerUpRight));
    MPI_Isend(&(temp[width*(width-1)]), 1,     MPI_INT,      id_down_left,  Direction.DOWN_LEFT,  MPI_COMM_WORLD, &(*sHandlerDownLeft));
    MPI_Isend(&(temp[width*width-1]),   1,     MPI_INT,      id_down_right, Direction.DOWN_RIGHT, MPI_COMM_WORLD, &(*sHandlerDownRight));

}


// TO CHECK: reversed Directions tags
void receivePeripheralsFromNeighbours(int myid, int width, int numprocs, int *up_buffer, int *down_buffer, int *left_buffer,
                                      int *right_buffer, int *up_left_buffer, int *up_right_buffer, int *down_left_buffer, int *down_right_buffer,
                                      MPI_Request *rHandlerUp, MPI_Request *rHandlerDown, MPI_Request *rHandlerLeft, MPI_Request *rHandlerRight,
                                      MPI_Request *rHandlerUpLeft, MPI_Request *rHandlerUpRight, MPI_Request *rHandlerDownLeft, MPI_Request *rHandlerDownRight){

    int id_up         = proc_id_up        (myid,(int)sqrt(numprocs), (int)sqrt(numprocs), numprocs);
    int id_down       = proc_id_down      (myid,(int)sqrt(numprocs), (int)sqrt(numprocs), numprocs);
    int id_right      = proc_id_right     (myid,(int)sqrt(numprocs), (int)sqrt(numprocs), numprocs);
    int id_left       = proc_id_left      (myid,(int)sqrt(numprocs), (int)sqrt(numprocs), numprocs);
    int id_up_left    = proc_id_up_left   (myid,(int)sqrt(numprocs), (int)sqrt(numprocs), numprocs);
    int id_up_right   = proc_id_up_right  (myid,(int)sqrt(numprocs), (int)sqrt(numprocs), numprocs);
    int id_down_left  = proc_id_down_left (myid,(int)sqrt(numprocs), (int)sqrt(numprocs), numprocs);
    int id_down_right = proc_id_down_right(myid,(int)sqrt(numprocs), (int)sqrt(numprocs), numprocs);



    MPI_Irecv(up_buffer,         width, MPI_INT, id_up ,        Direction.DOWN,       MPI_COMM_WORLD, &(*rHandlerUp));
    MPI_Irecv(down_buffer,       width, MPI_INT, id_down,       Direction.UP,         MPI_COMM_WORLD, &(*rHandlerDown));
    MPI_Irecv(left_buffer,       width, MPI_INT, id_left,       Direction.RIGHT,      MPI_COMM_WORLD, &(*rHandlerLeft));
    MPI_Irecv(right_buffer,      width, MPI_INT, id_right,      Direction.LEFT,       MPI_COMM_WORLD, &(*rHandlerRight));
    MPI_Irecv(up_left_buffer,    1,     MPI_INT, id_up_left,    Direction.DOWN_RIGHT, MPI_COMM_WORLD, &(*rHandlerUpLeft));
    MPI_Irecv(up_right_buffer,   1,     MPI_INT, id_up_right,   Direction.DOWN_LEFT,  MPI_COMM_WORLD, &(*rHandlerUpRight));
    MPI_Irecv(down_left_buffer,  1,     MPI_INT, id_down_left,  Direction.UP_RIGHT,   MPI_COMM_WORLD, &(*rHandlerDownLeft));
    MPI_Irecv(down_right_buffer, 1,     MPI_INT, id_down_right, Direction.UP_LEFT,    MPI_COMM_WORLD, &(*rHandlerDownRight));

}

void sendLocalStateToMaster(int *temp, int size){
    MPI_Send(temp, size, MPI_INT, MASTER_PROC_ID, TAG_PRINT, MPI_COMM_WORLD);
}


void receiveAllStates(int numprocs,int ***in, int width) {
    int i;
    int line, column;


    for (i=1; i<numprocs; i++){
        line=i/(int) sqrt(numprocs);
        column=mod(i,(int) sqrt(numprocs));

        in[line][column]=malloc(sizeof (int)*width*width);

        MPI_Recv(in[line][column],width*width,MPI_INT,i,TAG_PRINT,MPI_COMM_WORLD,MPI_STATUS_IGNORE);

    }
}

void finalizeCommunications(MPI_Request *sHandlerUp, MPI_Request *sHandlerDown, MPI_Request *sHandlerLeft, MPI_Request *sHandlerRight,
                            MPI_Request *sHandlerUpLeft, MPI_Request *sHandlerUpRight, MPI_Request *sHandlerDownLeft, MPI_Request *sHandlerDownRight,
                            MPI_Request *rHandlerUp, MPI_Request *rHandlerDown, MPI_Request *rHandlerLeft, MPI_Request *rHandlerRight,
                            MPI_Request *rHandlerUpLeft, MPI_Request *rHandlerUpRight, MPI_Request *rHandlerDownLeft, MPI_Request *rHandlerDownRight){

    MPI_Status status;

    MPI_Wait(&(*sHandlerUp),        &status);
    MPI_Wait(&(*sHandlerDown),      &status);
    MPI_Wait(&(*sHandlerLeft),      &status);
    MPI_Wait(&(*sHandlerRight),     &status);
    MPI_Wait(&(*sHandlerUpLeft),    &status);
    MPI_Wait(&(*sHandlerUpRight),   &status);
    MPI_Wait(&(*sHandlerDownLeft),  &status);
    MPI_Wait(&(*sHandlerDownRight), &status);

    MPI_Wait(&(*rHandlerUp),        &status);
    MPI_Wait(&(*rHandlerDown),      &status);
    MPI_Wait(&(*rHandlerLeft),      &status);
    MPI_Wait(&(*rHandlerRight),     &status);
    MPI_Wait(&(*rHandlerUpLeft),    &status);
    MPI_Wait(&(*rHandlerUpRight),   &status);
    MPI_Wait(&(*rHandlerDownLeft),  &status);
    MPI_Wait(&(*rHandlerDownRight), &status);

}