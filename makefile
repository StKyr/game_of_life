CC = mpicc
NVCC = nvcc
OMP_FLAGS = -fopenmp
EXTRA_PAR = -lm

game: game.o lib.o
	$(CC) $(OMP_FLAGS) -o game game.o lib.o $(EXTRA_PAR)

mpi: game.o lib.o
	$(CC) -o game_mpi game.o lib.o $(EXTRA_PAR)

game.o: game.c
	$(CC) -c game.c

lib.o: lib.c
	$(CC) -c lib.c

cuda: game-of-life.cu
	$(NVCC) -o game_cuda game-of-life.cu

alias:
	mpi, cuda, clean

clean:
	rm -f game.o lib.o game game_mpi game_cuda
