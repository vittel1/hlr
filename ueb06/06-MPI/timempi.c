#define _GNU_SOURCE
#include <stdio.h>
#include <mpi.h>
#include <sys/time.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>

int main(int argc, char** argv)
{
	MPI_Init(&argc, &argv);
	int world_size, world_rank;
	MPI_Comm_size(MPI_COMM_WORLD, &world_size);
	MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);

	if(world_rank != 0)
	{
		struct timeval t;
		time_t curtime;
		char buffer_time[30];
		char* time_with_ms;

		gettimeofday(&t, NULL);
		curtime = t.tv_sec;
		strftime(buffer_time, 30, "%Y-%m-%d %T", localtime(&curtime));
		asprintf(&time_with_ms, "%s.%ld", buffer_time, t.tv_usec);

		char hostname[8];
		hostname[7] = '\0';
		gethostname(hostname, 8);

		char* msg;
		asprintf(&msg, "%s: %s \n", hostname, time_with_ms);
		//Message_send
		MPI_Send(msg, (strlen(msg)+1), MPI_CHAR, 0, 0, MPI_COMM_WORLD);
		free(msg);
		free(time_with_ms);		
		//barrier
		MPI_Barrier(MPI_COMM_WORLD);
		//fertig
	}
	else
	{
		for(int i = 1; i < world_size; i++)
		{
			char* buf ="";
			//probeMessage
			MPI_Status status;
			int nbytes;

			MPI_Probe(i, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
			MPI_Get_count(&status, MPI_CHAR, &nbytes);
			
			//buffer einstellen
			if(nbytes != MPI_UNDEFINED)
			{
				buf = malloc(nbytes);
			}
			//Message_recv
			MPI_Recv(buf, nbytes, MPI_CHAR, status.MPI_SOURCE, status.MPI_TAG, MPI_COMM_WORLD, &status);
			//print
			printf(buf);
			free(buf);
		}
		//auch an die barrier
		MPI_Barrier(MPI_COMM_WORLD);
		//fertig
	}

	printf("Rang %d beendet jetzt.\n", world_rank);

	MPI_Finalize();
}
