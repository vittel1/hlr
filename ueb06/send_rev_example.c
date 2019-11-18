#include <mpi.h>
#include <stdio.h>


int main(int argc, char *argv[])
{
        int  numtasks, rank, len, rc;
        char hostname[MPI_MAX_PROCESSOR_NAME];
        //numtasks = 4;
        // initialize MPI
        MPI_Init(&argc,&argv);

        // get number of tasks
        MPI_Comm_size(MPI_COMM_WORLD,&numtasks);

        // get my rank
        MPI_Comm_rank(MPI_COMM_WORLD,&rank);

        // this one is obvious
        MPI_Get_processor_name(hostname, &len);
        printf ("Number of tasks= %d My rank= %d Running on %s\n", numtasks,rank,hostname);


        // do some work with message passing
	if(rank == 1)
	{
		//data, count, datatype, destination, tag, mpi_comm
		MPI_Send(hostname, 1, MPI_BYTE, 0, 1, MPI_COMM_WORLD);
	} else if (rank == 0) 
	{
		//data, count, datatype, source, tag, mpi_comm, status*
		MPI_Recv(hostname, 1, MPI_BYTE, 1, 1, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
		printf("Process 0 received Hostname %s from process 0\n", hostname);
	}

        // done with MPI
        MPI_Finalize();
}
