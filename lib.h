#ifndef _MY_LIB_H_
#define _MY_LIB_H_

#include <stdio.h>
#include "mpi.h"

#define MASTER_PROC_ID 0
#define CREATURE_SYMBOL 'O'
#define EMPTY_SYMBOL '.'
#define TAG_PRINT 23
#define TAG_INIT 46


struct params{
    int Cols;
    int Rows;
    int max_iterations;               // use -1 for unlimited
    int should_print;
    int numthreads;                   // use -1 for default
    int alive_probability;           // use -1 for default
} Params;



// auxiliary functions
int I_AM_MASTER(int myid);
int I_AM_SLAVE(int myid);
int mod(int a, int b);

void parseCommandLineArguments(int argc, char* argv[]);

// game ruling functions
void printState(int numprocs, int width, int *** board);                                  // prints current state of the board
void initializeBoard(int *subboard, int dimX, int dimY, int prob);                                  // place creatures on the board

void receiveAllStates(int numprocs,int ***in, int width);                                 // get all subtables for printing

// get the number of alive neighbours of a specific cell
int countNeighboursOuter(int i, const int *temp, int width, int *up_buffer,
                                                 int *down_buffer,
                                                 int *left_buffer,
                                                 int *right_buffer,
                                                 int up_left_buffer,
                                                 int up_right_buffer,
                                                 int down_left_buffer,
                                                 int down_right_buffer);
int countNeighboursInner(int i, const int *temp, int width);


int isOuter(int i, int width);



int updateLocalState(const int *sums, int *temp, int width);         // change local subtable to next state
int checkGlobalStateChanged(int myState);                            // check if at least one process had a change
void sendLocalStateToMaster(int *temp, int size);
void sendPeripheralsToNeighbours(int myid, int *temp, int width, int numprocs,
                                 MPI_Request *sHandlerUp, MPI_Request *sHandlerDown, MPI_Request *sHandlerLeft, MPI_Request *sHandlerRight,
                                 MPI_Request *sHandlerUpLeft, MPI_Request *sHandlerUpRight, MPI_Request *sHandlerDownLeft, MPI_Request *sHandlerDownRight);

void receivePeripheralsFromNeighbours(int myid, int width, int numprocs, int *up_buffer,
                                                                         int *down_buffer,
                                                                         int *left_buffer,
                                                                         int *right_buffer,
                                                                         int *up_left_buffer,
                                                                         int *up_right_buffer,
                                                                         int *down_left_buffer,
                                                                         int *down_right_buffer,
                                      MPI_Request *rHandlerUp, MPI_Request *rHandlerDown, MPI_Request *rHandlerLeft, MPI_Request *rHandlerRight,
                                      MPI_Request *rHandlerUpLeft, MPI_Request *rHandlerUpRight, MPI_Request *rHandlerDownLeft, MPI_Request *rHandlerDownRight);

void finalizeCommunications(MPI_Request *sHandlerUp, MPI_Request *sHandlerDown, MPI_Request *sHandlerLeft, MPI_Request *sHandlerRight,
                            MPI_Request *sHandlerUpLeft, MPI_Request *sHandlerUpRight, MPI_Request *sHandlerDownLeft, MPI_Request *sHandlerDownRight,
                            MPI_Request *rHandlerUp, MPI_Request *rHandlerDown, MPI_Request *rHandlerLeft, MPI_Request *rHandlerRight,
                            MPI_Request *rHandlerUpLeft, MPI_Request *rHandlerUpRight, MPI_Request *rHandlerDownLeft, MPI_Request *rHandlerDownRight
);





#endif