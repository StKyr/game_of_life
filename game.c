#include "mpi.h"
#include <unistd.h>
#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#include <omp.h>

#include "lib.h"








int main(int argc, char **argv)
{
    int myid, numprocs;
    int width;
    int i, j;
    int myChange, someChangeHappened = 1;
    int numIterations =0;

    MPI_Init(&argc,&argv);


    MPI_Comm_size(MPI_COMM_WORLD,&numprocs);
    MPI_Comm_rank(MPI_COMM_WORLD, &myid);

    parseCommandLineArguments(argc, argv);


    width = (int) sqrt(Params.Rows*Params.Cols/numprocs); //todo...
    srand(myid*(unsigned)time(NULL));

    int temp[width*width];
    int sums[width*width];
    int *** in;


    int up_buffer[width];
    int down_buffer[width];
    int left_buffer[width];
    int right_buffer[width];
    int up_left_buffer;
    int up_right_buffer;
    int down_left_buffer;
    int down_right_buffer;

    MPI_Request sHandlerUp, sHandlerDown, sHandlerLeft, sHandlerRight, sHandlerUpLeft, sHandlerUpRight, sHandlerDownLeft, sHandlerDownRight;
    MPI_Request rHandlerUp, rHandlerDown, rHandlerLeft, rHandlerRight, rHandlerUpLeft, rHandlerUpRight, rHandlerDownLeft, rHandlerDownRight;




    if (I_AM_MASTER(myid) && Params.should_print) {
        in = malloc(sizeof(int *) * ((int) sqrt(numprocs)));
        for (i = 0; i < (int) sqrt(numprocs); i++) in[i] = malloc(sizeof(int) * ((int) sqrt(numprocs)));
    }else{
        in = NULL;
    }


    initializeBoard(temp, width, width, Params.alive_probability);


    // max_iterations == -1 means infinite loops
    MPI_Barrier(MPI_COMM_WORLD);
    double start_t = MPI_Wtime();
    while(someChangeHappened && (  (Params.max_iterations == -1) ? 1 : numIterations < Params.max_iterations)){
        numIterations++;

        if (Params.should_print) {

            if (I_AM_MASTER(myid)) { // gather sub-tables to print

                in[0][0] = temp;
                if (numprocs != 1) receiveAllStates(numprocs, in, width);
                for (i = 0; i < 2; i++) { printf("\n"); }
                printState(numprocs, width, in);
                usleep(333 * 1000); // stalling for seeing live results on screen
            } else {
                sendLocalStateToMaster(temp, width * width); // send current state for printing
            }
        }


        sendPeripheralsToNeighbours(myid, temp, width, numprocs,
                                        &sHandlerUp, &sHandlerDown, &sHandlerLeft, &sHandlerRight, &sHandlerUpLeft, &sHandlerUpRight,
                                        &sHandlerDownLeft, &sHandlerDownRight);

        receivePeripheralsFromNeighbours(myid, width, numprocs,
                                         up_buffer, down_buffer, left_buffer, right_buffer, &up_left_buffer,
                                         &up_right_buffer, &down_left_buffer, &down_right_buffer,
                                         &rHandlerUp, &rHandlerDown, &rHandlerLeft, &rHandlerRight, &rHandlerUpLeft, &rHandlerUpRight,
                                         &rHandlerDownLeft, &rHandlerDownRight);


        if (Params.numthreads != -1) {

            //todo: documentation about reusage of threads
#pragma omp parallel for num_threads(Params.numthreads) \
            default(none) shared(width, temp, sums) private(i)
            for (i = 0; i < width * width; i++) {

                if (!isOuter(i, width)) {

                    sums[i] = countNeighboursInner(i, temp, width);
                }
            }
        }else{

#pragma omp parallel for \
            default(none) shared(width, temp, sums) private(i)
            for (i = 0; i < width * width; i++) {

                if (!isOuter(i, width)) {

                    sums[i] = countNeighboursInner(i, temp, width);
                }
            }

        }


        finalizeCommunications(
                &sHandlerUp, &sHandlerDown, &sHandlerLeft, &sHandlerRight, &sHandlerUpLeft, &sHandlerRight, &sHandlerDownLeft, &sHandlerRight,
                &rHandlerUp, &rHandlerDown, &rHandlerLeft, &rHandlerRight, &rHandlerUpLeft, &rHandlerRight, &rHandlerDownLeft, &rHandlerRight);



        for(i=0;i<width*width;i++) {

            if (isOuter(i, width)) {
                sums[i] = countNeighboursOuter(i, temp, width,
                                               up_buffer, down_buffer, left_buffer, right_buffer,
                                               up_left_buffer, up_right_buffer,
                                               down_left_buffer, down_right_buffer);
            }
        }


        myChange = updateLocalState(sums, temp, width);

        //todo: documentation here
        if (numIterations % 10 == 0) {
            someChangeHappened = checkGlobalStateChanged(myChange);
        }else{
            someChangeHappened = 1;
        }


    }// end of game loop

    MPI_Barrier(MPI_COMM_WORLD);
    double end_t = MPI_Wtime();

    double time_diff = end_t - start_t;
    double max_diff, min_diff;
    MPI_Allreduce(&time_diff, &max_diff, 1, MPI_DOUBLE, MPI_MAX, MPI_COMM_WORLD);
    MPI_Allreduce(&time_diff, &min_diff, 1, MPI_DOUBLE, MPI_MIN, MPI_COMM_WORLD);

    if (I_AM_MASTER(myid)) printf("Elapsed time (%d iterations): %f sec (max)\n", numIterations, max_diff);
    if (I_AM_MASTER(myid)) printf("Elapsed time (%d iterations): %f sec (master time)\n", numIterations, time_diff);
    if (I_AM_MASTER(myid)) printf("Elapsed time (%d iterations): %f sec (min)\n", numIterations, min_diff);

    MPI_Finalize();
    return 0;
}
