#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <mpi.h>


//Initialisiert Array des einzelnen Prozesses auf richtige Größe und korrigiert
//(falls nötig) den Größenwert, damit später vernünftig über das Array iteriert werden kann.
int*
init (int* size, int rank, int rest)
{
	int* buf;
	if(rest - rank > 0)
	{
		buf = malloc(sizeof(int) * (*size + 1));
		*size = *size + 1;
	}
	else
	{
		buf = malloc(sizeof(int) * *size);
	}
	
	//TODO
	//den random kram verändern, damit auch mal wirklich was passiert
	srand(rank);

	for (int i = 0; i < *size; i++)
	{
		// Do not modify "% 25"
		buf[i] = rand() % 25;
	}

	return buf;
}

void printArrays(int rank, int sizePerProcess, int world_size, int* buf)
{
	//wenn nicht erster rang, warte auf nachricht von vorgänger
	if(rank != 0)
	{
		int* x = malloc(sizeof(int));
		MPI_Recv(x, 1, MPI_INT, rank - 1, MPI_ANY_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
		free(x);
	}

	printf("rank %d enthält: %d\n", rank, sizePerProcess);
	//Array-Ausgabe
	for (int i = 0; i < sizePerProcess; i++)
	{
		printf("rank %d: %d\n", rank, buf[i]);
	}

	//wenn nicht letzter rang, schicke nachricht an nachfolger
	if(rank != world_size - 1)
	{
		MPI_Send(&rank, 1, MPI_INT, rank + 1, 0, MPI_COMM_WORLD);
	}
}

void
circle (int** buf, int* size, int world_size, int rank)
{
	// TODO

	int left, right, stopValue, breakLoop;
	MPI_Status status;

	left = (rank == 0) ? (world_size - 1) : (rank - 1);
	right = (rank == (world_size - 1)) ? 0 : (rank + 1);
	
	//0 schickt abbruchbedingung an rank-1
	if(rank == 0)
	{
		int first = (*buf)[0];
		MPI_Send(&first, 1, MPI_INT, world_size - 1, 0, MPI_COMM_WORLD);
	}
	else if(rank == world_size - 1)
	{	
		MPI_Recv(&stopValue, 1, MPI_INT, 0, MPI_ANY_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
	}

	//schleife:
	while(!breakLoop)
	{
		//0 sendet, dann probe und recv, anschließend buf überschreiben
		if(rank == 0)
		{
			int* temp;

			MPI_Send(*buf, *size, MPI_INT, right, 0, MPI_COMM_WORLD);
		
			MPI_Probe(left, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
			MPI_Get_count(&status, MPI_INT, size);

			if(*size != MPI_UNDEFINED)
			{
				temp = malloc(sizeof(int) * *size);
			}

			MPI_Recv(temp, *size, MPI_INT, status.MPI_SOURCE, status.MPI_TAG, MPI_COMM_WORLD, &status);
			free(*buf);
			*buf = temp;
		}
		//alle anderen andersrum:
		//erst probe und recv, zwischenspeichern, senden und buf überschreiben
		else
		{
			int* temp;
			int tempSize;

			MPI_Probe(left, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
			MPI_Get_count(&status, MPI_INT, &tempSize);

			if(tempSize != MPI_UNDEFINED)
			{
				temp = malloc(sizeof(int) * tempSize);
			}

			MPI_Recv(temp, tempSize, MPI_INT, status.MPI_SOURCE, status.MPI_TAG, MPI_COMM_WORLD, &status);

			MPI_Send(*buf, *size, MPI_INT, right, 0, MPI_COMM_WORLD);
			free(*buf);
			*buf = temp;
			*size = tempSize;
		}
		//barrier und prüfen lassen, ob alle aufhören
	
		MPI_Barrier(MPI_COMM_WORLD);
		if(rank == world_size - 1)
		{
			breakLoop = ((*buf)[0] == stopValue) ? 1 : 0;
		}
		//broadcast vom letzten, um zu gucken, ob ende
		//wenn ja, endet die Schleife
		MPI_Bcast(&breakLoop, 1, MPI_INT, world_size - 1, MPI_COMM_WORLD);
		
	}


}

int
main (int argc, char** argv)
{
	MPI_Init(&argc, &argv);
	int N;
	int rank;
	int world_size;
	int* buf;
	int rest = 0;

	MPI_Comm_size(MPI_COMM_WORLD, &world_size);
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);

	if (argc < 2)
	{
		printf("Arguments error!\nPlease specify a buffer size.\n");
		return EXIT_FAILURE;
	}

	N = atoi(argv[1]);
	if(N < 1)
	{
		printf("Gültige Arraygröße eingeben\n");
		return EXIT_FAILURE;
	}
	//Restberechnung
	int sizePerProcess = N / world_size;
	if(sizePerProcess * world_size != N)
	{
		rest = N - sizePerProcess * world_size;
	}

	// Array length
	buf = init(&sizePerProcess, rank, rest);

	if(rank == 0)
	{
		printf("\nBEFORE\n");
	}

	printArrays(rank, sizePerProcess, world_size, buf);

	MPI_Barrier(MPI_COMM_WORLD);

	circle(&buf, &sizePerProcess, world_size, rank);

	MPI_Barrier(MPI_COMM_WORLD);

	if(rank == 0)
	{
		printf("\nAFTER\n");
	}

	printArrays(rank, sizePerProcess, world_size, buf);

	MPI_Barrier(MPI_COMM_WORLD);

	MPI_Finalize();

	return EXIT_SUCCESS;
}
