/****************************************************************************/
/****************************************************************************/
/**                                                                        **/
/**                 TU München - Institut für Informatik                   **/
/**                                                                        **/
/** Copyright: Prof. Dr. Thomas Ludwig                                     **/
/**            Andreas C. Schmidt                                          **/
/**                                                                        **/
/** File:      partdiff.c                                                  **/
/**                                                                        **/
/** Purpose:   Partial differential equation solver for Gauß-Seidel and    **/
/**            Jacobi method.                                              **/
/**                                                                        **/
/****************************************************************************/
/****************************************************************************/

/* ************************************************************************ */
/* Include standard header file.                                            */
/* ************************************************************************ */
#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <math.h>
#include <malloc.h>
#include <sys/time.h>
#include <mpi.h>

#include "partdiff.h"

struct calculation_arguments
{
	uint64_t  N;              /* number of spaces between lines (lines=N+1)     */
	uint64_t  num_matrices;   /* number of matrices                             */
	double    h;              /* length of a space between two lines            */
	double    ***Matrix;      /* index matrix used for addressing M             */
	double    *M;             /* two matrices with real values                  */
};

struct calculation_results
{
	uint64_t  m;
	uint64_t  stat_iteration; /* number of current iteration                    */
	double    stat_precision; /* actual precision of all slaves in iteration    */
};

/* ************************************************************************ */
/* Global variables                                                         */
/* ************************************************************************ */

/* time measurement variables */
struct timeval start_time;       /* time when program started                      */
struct timeval comp_time;        /* time when calculation completed                */


/* ************************************************************************ */
/* initVariables: Initializes some global variables                         */
/* ************************************************************************ */
static
void
initVariables (struct calculation_arguments* arguments, struct calculation_results* results, struct options const* options, struct thread_data* data)
{
	arguments->N = (options->interlines * 8) + 9 - 1;

	int N = arguments->N;

	//Falls es mehr Prozesse als Matrixzeilen gibt, brich ab
	if(N < data->world_size && data->rank == 0)
	{
		printf("Zu viele Prozesse für zu wenig Interlines, Abbruch\n");
		exit(1);
	}

	if(options->method == METH_GAUSS_SEIDEL && N < 2 * data->world_size && data->rank == 0)
	{
		printf("Zu viele Prozesse für zu wenig Interlines, Abbruch\n");
		exit(1);
	}
	arguments->num_matrices = (options->method == METH_JACOBI) ? 2 : 1;
	arguments->h = 1.0 / arguments->N;

	results->m = 0;
	results->stat_iteration = 0;
	results->stat_precision = 0;

	//Berechne die Anzahl der Lines für jeden Prozess und verteile Reste
	data->numLines = (N+1) / data->world_size;
	int rest = (N+1) % data->world_size;
	if(rest - data->rank > 0)
	{
		data->numLines = data->numLines + 1;
	}

	//Jeder Prozess kriegt seinen Start- und Endindex (global gesehen)
	//Hier ist darauf zu achten, dass nicht alle Prozesse gleich viele Lines haben
	if(data->rank < rest)
	{
		data->globalStart = data->rank * data->numLines;
	}
	else
	{
		data->globalStart = (data->numLines + 1) * rest + (data->rank - rest) * data->numLines;
	}

	data->globalEnd = data->globalStart + data->numLines - 1;	

}

/* ************************************************************************ */
/* freeMatrices: frees memory for matrices                                  */
/* ************************************************************************ */
static
void
freeMatrices (struct calculation_arguments* arguments)
{
	uint64_t i;

	for (i = 0; i < arguments->num_matrices; i++)
	{
		free(arguments->Matrix[i]);
	}

	free(arguments->Matrix);
	free(arguments->M);
}

/* ************************************************************************ */
/* allocateMemory ()                                                        */
/* allocates memory and quits if there was a memory allocation problem      */
/* ************************************************************************ */
static
void*
allocateMemory (size_t size)
{
	void *p;

	if ((p = malloc(size)) == NULL)
	{
		printf("Speicherprobleme! (%" PRIu64 " Bytes angefordert)\n", size);
		exit(1);
	}

	return p;
}

/* ************************************************************************ */
/* allocateMatrices: allocates memory for matrices                          */
/* ************************************************************************ */

/*Anstatt hier beide Matrizen zu allokieren, kriegt jeder Prozess nur den
Speicher für seine Lines auf den beiden Matrizen.*/
static
void
allocateMatrices (struct calculation_arguments* arguments, struct thread_data* data)
{
	uint64_t i, j;

	uint64_t numLines = data->numLines;

	uint64_t const N = arguments->N;

	arguments->M = allocateMemory(arguments->num_matrices * numLines * (N + 1) * sizeof(double));
	arguments->Matrix = allocateMemory(arguments->num_matrices * sizeof(double**));

	for (i = 0; i < arguments->num_matrices; i++)
	{
		arguments->Matrix[i] = allocateMemory(numLines * sizeof(double*));

		for (j = 0; j < numLines; j++)
		{
			arguments->Matrix[i][j] = arguments->M + (i * numLines * (N + 1)) + (j * (N + 1));
		}
	}
}

/* ************************************************************************ */
/* initMatrices: Initialize matrix/matrices and some global variables       */
/* ************************************************************************ */
static
void
initMatrices (struct calculation_arguments* arguments, struct options const* options, struct thread_data* data)
{
	uint64_t g, i, j;                                /*  local variables for loops   */

	uint64_t const N = arguments->N;
	double const h = arguments->h;
	uint64_t numLines = data->numLines;
	double*** Matrix = arguments->Matrix;

	/* initialize matrix/matrices with zeros */
	for (g = 0; g < arguments->num_matrices; g++)
	{
		for (i = 0; i < numLines; i++)
		{
			for (j = 0; j <= N; j++)
			{
				Matrix[g][i][j] = 0.0;
			}
		}
	}

	/* initialize borders, depending on function (function 2: nothing to do) */
	if (options->inf_func == FUNC_F0)
	{
		/*for (g = 0; g < arguments->num_matrices; g++)
		{
			for (i = 0; i <= N; i++)
			{
				Matrix[g][i][0] = 1.0 - (h * i);
				Matrix[g][i][N] = h * i;
				Matrix[g][0][i] = 1.0 - (h * i);
				Matrix[g][N][i] = h * i;
			}

			Matrix[g][N][0] = 0.0;
			Matrix[g][0][N] = 0.0;*/

			//erste Reihe initialisieren
			if(data->rank == 0)
			{
				for (g = 0; g < arguments->num_matrices; g++)
				{
					for (i = 0; i <= N; i++)
					{
						Matrix[g][0][i] = 1.0 - (h * i);
					}
					Matrix[g][0][N] = 0.0;
				}
			}

			//letzte Reihe initialisieren
			if(data->rank == (data->world_size - 1))
			{
				for (g = 0; g < arguments->num_matrices; g++)
				{
					for (i = 0; i <= N; i++)
					{
						Matrix[g][(numLines - 1)][i] = h * i;
					}
					Matrix[g][(numLines - 1)][0] = 0.0;
				}
			}

			//die senkrechten Ränder initialisieren, hier auf das i immer den
			//globalStart addieren, weil die Schleife nicht mehr über ganze Matrix geht
			for(g = 0; g < arguments->num_matrices; g++)
			{
				for(i = 0; i < numLines; i++)
				{
					Matrix[g][i][0] = 1.0 - (h* (i + data->globalStart));
					Matrix[g][i][N] = h * (i + data->globalStart);
				}
			}
	}
}


/*Falls das Gauss-Seidel-Verfahren und Abbruch nach Genauigkeit gewählt wurden, übernimmt
 * ein Prozess die Verwaltung der maxresiduums und benachrichtigt alle Prozesse, wenn die
 * geforderte Präzision erreicht wurde.*/
static
void
managePrecision(struct options const* options, struct thread_data* data)
{
	double term_precision = options->term_precision;
	double maxresiduum;
	int abort = 0;
	int world_size = data->world_size;
	int count = 0;

	while(1)
	{
		double maxresiduumOneIteration = 0.0;
		int processesToInformAboutAborting[world_size];

		//erhalte das maxresiduum von jedem Prozess und vergleiche mit Präzision.
		//Falls das maxresiduum eines Prozesses unter der Grenze ist, muss er darüber
		//informiert werden, ob alle anderen die Genauigkeit auch erreicht haben oder nicht
		for (int i = 0; i < world_size; i++)
		{
			double newresiduum;
			MPI_Recv(&newresiduum, 1, MPI_DOUBLE, i, count, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
			
			if(newresiduum < term_precision)
			{
				processesToInformAboutAborting[i] = 1;
			}
			else
			{
				processesToInformAboutAborting[i] = 0;
			}


			if(newresiduum > maxresiduumOneIteration)
			{
				maxresiduumOneIteration = newresiduum;
			}	
		}

		maxresiduum = maxresiduumOneIteration;

		//Falls alle maxresiduums unter der geforderten Genauigkeit sind, soll
		//das Verfahren terminieren
		if(maxresiduum < term_precision)
		{
			abort = 1;
		}

		for(int i = 0; i < world_size; i++)
		{
			if(processesToInformAboutAborting[i] == 1)
			{
				MPI_Send(&abort, 1, MPI_INT, i, 0, MPI_COMM_WORLD); 
			}
		}

		//Wenn das Verfahren terminieren soll, werden alle Prozesse über das globale maxresiduum
		//informiert und danach beendet sich dieser Verwaltungsprozess
		if(abort == 1)
		{
			for(int i = 0; i < world_size; i++)
			{
				MPI_Send(&maxresiduum, 1, MPI_DOUBLE, i, 0, MPI_COMM_WORLD);
			}
			MPI_Barrier(MPI_COMM_WORLD);
			MPI_Finalize();
			exit(0);
		}

		count++;
	}
}

static
void
calculateGaussMPI (struct calculation_arguments const* arguments, struct calculation_results* results, struct options const* options, struct thread_data* data)
{
	int i, j;                                   /* local variables for loops */
	double star;                                /* four times center value minus 4 neigh.b values */
	double residuum;                            /* residuum of current iteration */
	double maxresiduum;                         /* maximum residuum value of a slave in iteration */

	int const N = arguments->N;
	double const h = arguments->h;

	MPI_Request req;

	double pih = 0.0;
	double fpisin = 0.0;

	int term_iteration = options->term_iteration;

	int rank = data->rank;
	int lines = data->numLines;

	int predecessor = rank - 1;
	int successor = rank + 1;

	double** Matrix = arguments->Matrix[0];

	if (options->inf_func == FUNC_FPISIN)
	{
		pih = PI * h;
		fpisin = 0.25 * TWO_PI_SQUARE * h * h;
	}

	double* bufNewFirstLine = malloc(sizeof(double) * (N+1)); /*Hier wird die empfangene Zeile des Vorgängers gespeichert*/
	double* bufNewLastLine = malloc(sizeof(double) * (N+1)); /*Hier wird die empfangene Zeile des Nachfolgers gespeichert*/

	//In der folgenden Matrix werden sowohl die eigenen Zeilen als auch die jeweils empfangenen gespeichert
	int linesComplete;
	double** matrixWithHaloLines;
	if(rank == 0 || rank == (data->world_size - 1))
	{
		linesComplete = lines + 1;
		matrixWithHaloLines = malloc(sizeof(double*) * linesComplete);
	}
	else
	{
		linesComplete = lines + 2;
		matrixWithHaloLines = malloc(sizeof(double*) * linesComplete);
	}

	//Für den Anfang soll jeder Prozess != 0 seine erste Zeile an seinen Vorgänger senden.
	//Ebenfalls initialisiert jeder Prozess seine neue Matrix.
	if(rank != 0)
	{
		double* firstLine = Matrix[0];
		MPI_Isend(firstLine, N + 1, MPI_DOUBLE, predecessor, results->stat_iteration, MPI_COMM_WORLD, &req);
		for(int x = 0; x < lines; x++)
		{
			matrixWithHaloLines[x + 1] = Matrix[x];
		}
	}
	else
	{
		for(int x = 0; x < linesComplete - 1; x++)
		{
			matrixWithHaloLines[x] = Matrix[x];
		}
	}

	while (term_iteration > 0)
	{

		maxresiduum = 0;

		/* over all rows */
		for (i = 1; i < (linesComplete - 1); i++)
		{

			//Falls für eine Iteration die letzte Zeile des Vorgängers benötigt wird, empfange sie hier
			if(i == 1 && rank != 0)
			{
				MPI_Recv(bufNewFirstLine, N + 1, MPI_DOUBLE, predecessor, results->stat_iteration, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
				matrixWithHaloLines[0] = bufNewFirstLine;
			}

			//Falls für eine Iteration die erste Zeile des Nachfolgers benötigt wird, empfange sie hier
			if(i == (linesComplete - 2) && rank != (data->world_size - 1))
			{
				MPI_Recv(bufNewLastLine, N + 1, MPI_DOUBLE, successor, results->stat_iteration, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
				matrixWithHaloLines[linesComplete - 1] = bufNewLastLine;
			}

			double fpisin_i = 0.0;

			if (options->inf_func == FUNC_FPISIN)
			{
				//da die Schleife nicht mehr über die gesamte Matrix geht, muss i angepasst werden;
				//für alle Prozesse außer den ersten muss der globale Start draufgerechnet werden
				double adaptedI = rank == 0 ? (double) i : (double)(i - 1 + data->globalStart);
				//fpisin_i = fpisin * sin(pih * (double)i);
				fpisin_i = fpisin * sin(pih * adaptedI);
			}

			/* over all columns */
			for (j = 1; j < N; j++)
			{
				star = 0.25 * (matrixWithHaloLines[i-1][j] + matrixWithHaloLines[i][j-1] + matrixWithHaloLines[i][j+1] + matrixWithHaloLines[i+1][j]);

				if (options->inf_func == FUNC_FPISIN)
				{
					star += fpisin_i * sin(pih * (double)j);
				}

				if (options->termination == TERM_PREC || term_iteration == 1)
				{
					residuum = matrixWithHaloLines[i][j] - star;
					residuum = (residuum < 0) ? -residuum : residuum;
					maxresiduum = (residuum < maxresiduum) ? maxresiduum : residuum;
				}

				matrixWithHaloLines[i][j] = star;
			}

			//Sobald die eigene erste Zeile fertig berechnet ist, sende sie an den Vorgänger
			if(i == 1 && rank != 0)
			{
				double* sendFirstLine = matrixWithHaloLines[1];
				MPI_Isend(sendFirstLine, N + 1, MPI_DOUBLE, predecessor, (results->stat_iteration + 1), MPI_COMM_WORLD, &req);
			}

			//Sobald die letzte eigene Zeile fertig berechnet ist, sende sie an den Nachfolger
			if(i == (linesComplete - 2) && rank != (data->world_size - 1))
			{
				double* sendLastLine = matrixWithHaloLines[linesComplete - 2];
				MPI_Isend(sendLastLine, N + 1, MPI_DOUBLE, successor, results->stat_iteration, MPI_COMM_WORLD, &req);
			}
		}


		//Wenn nach Genauigkeit abgebrochen werden soll, sende erst das lokale maxresiduum an den Verwaltungsprozess.
		//Anschließend wird, falls das lokale maxresiduum unter der geforderten Genauigkeit liegt, eine Nachricht vom
		//Verwaltungsprozess abgewartet, ob terminiert werden soll oder nicht.
		if (options->termination == TERM_PREC)
		{
			MPI_Send(&maxresiduum, 1, MPI_DOUBLE, data->world_size, results->stat_iteration, MPI_COMM_WORLD);
			if (maxresiduum < options->term_precision)
			{
				int abort;
				MPI_Recv(&abort, 1, MPI_INT, data->world_size, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
				if(abort)
				{
					term_iteration = 0;
					MPI_Recv(&maxresiduum, 1, MPI_DOUBLE, data->world_size, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
				}
			}
			
		}
		//Beim Abbruch nach Iteration wird in der letzten Iteration das maxresiduum reduziert.
		else if (options->termination == TERM_ITER)
		{
			if(term_iteration == 1)
			{
				double reducedMaxresiduum;
				MPI_Allreduce(&maxresiduum, &reducedMaxresiduum, 1, MPI_DOUBLE, MPI_MAX, MPI_COMM_WORLD);
				maxresiduum = reducedMaxresiduum;
			}
			term_iteration--;
		}

		results->stat_iteration++;
		results->stat_precision = maxresiduum;

	}

	results->m = 0;

	free(bufNewLastLine);
	free(bufNewFirstLine);
	free(matrixWithHaloLines);
}

static
void
calculateJacobiMPI(struct calculation_arguments const* arguments, struct calculation_results* results, struct options const* options, struct thread_data* data)
{
	int i, j;                                   /* local variables for loops */
	int m1, m2;                                 /* used as indices for old and new matrices */
	double star;                                /* four times center value minus 4 neigh.b values */
	double residuum;                            /* residuum of current iteration */
	double maxresiduum;                         /* maximum residuum value of a slave in iteration */

	int const N = arguments->N;
	double const h = arguments->h;

	double pih = 0.0;
	double fpisin = 0.0;

	int term_iteration = options->term_iteration;

	//damit irec und isend verwendet werden können, werden einige Request- und
	//Status-Variablen gebraucht
	//Namensschema: 1. request oder status
	//		2. senden oder empfangen
	//		3. vom Vorgänger(Pre) oder vom Nachfolger(Post)
	MPI_Request requestRecPost;
	MPI_Request requestRecPre;
	MPI_Request requestSendPost;
	MPI_Request requestSendPre;

	MPI_Status statusRecPost;
	MPI_Status statusRecPre;
	MPI_Status statusSendPost;
	MPI_Status statusSendPre;

	double* bufPre = malloc(sizeof(double) * (N+1)); /*Hier wird die empfangene Zeile des Vorgängers gespeichert*/
	double* bufPost = malloc(sizeof(double) * (N+1)); /*Hier wird die empfangene Zeile des Nachfolgers gespeichert*/

	int rank = data->rank;
	int lines = data->numLines;

	//hier wird Zeilenanzahl der Matrix + empfangene Zeilen gespeichert
	int linesComplete;

	int post = rank + 1;
	int pre = rank - 1;

	//in dieser Matrix werden die eigentliche Matrix + die Zeilen der Vor-/Nachfolger
	//gespeichert
	double** matrixComplete;
	if(rank == 0 || rank == (data->world_size - 1))
	{
		linesComplete = lines + 1;
		matrixComplete = malloc(sizeof(double*) * linesComplete);
	}
	else
	{
		linesComplete = lines + 2;
		matrixComplete = malloc(sizeof(double*) * linesComplete);
	}


	/* initialize m1 and m2 depending on algorithm */
	if (options->method == METH_JACOBI)
	{
		m1 = 0;
		m2 = 1;
	}
	else
	{
		m1 = 0;
		m2 = 0;
	}

	if (options->inf_func == FUNC_FPISIN)
	{
		pih = PI * h;
		fpisin = 0.25 * TWO_PI_SQUARE * h * h;
	}

	while (term_iteration > 0)
	{
		double** Matrix_Out = arguments->Matrix[m1];
		double** Matrix_In  = arguments->Matrix[m2];

		double* firstLine = Matrix_In[0];
		double* lastLine = Matrix_In[lines-1];

		//rank 0 schickt letzte Zeile an 1 und empfängt von die erste von 1
		if(rank == 0)
		{
			MPI_Irecv(bufPost, N + 1, MPI_DOUBLE, post, 0, MPI_COMM_WORLD, &requestRecPost);
			MPI_Isend(lastLine, N + 1, MPI_DOUBLE, post, 0, MPI_COMM_WORLD, &requestSendPost);
			MPI_Wait(&requestRecPost, &statusRecPost);

			//Matrix_In und die empfangene Zeile werden in matrixComplete gespeichert
			matrixComplete[lines] = bufPost;
			for(int x = 0; x < lines; x++)
			{
				matrixComplete[x] = Matrix_In[x];
			}

			MPI_Wait(&requestSendPost, &statusSendPost);
		}
		//letzter Prozess schickt erste Zeile an letzter-1 und empfängt von letzter-1
		else if(rank == (data->world_size - 1))
		{
			MPI_Irecv(bufPre, N + 1, MPI_DOUBLE, pre, 0, MPI_COMM_WORLD, &requestRecPre);
			MPI_Isend(firstLine, N + 1, MPI_DOUBLE, pre, 0, MPI_COMM_WORLD, &requestSendPre);
			MPI_Wait(&requestRecPre, &statusRecPre);

			matrixComplete[0] = bufPre;
			for(int x = 1; x <= lines; x++)
			{
				matrixComplete[x] = Matrix_In[x - 1];
			}

			MPI_Wait(&requestSendPre, &statusSendPre);
		}
		//alle anderen müssen an Vorgänger und Nachfolger senden & empfangen
		else
		{
			MPI_Irecv(bufPre, N + 1, MPI_DOUBLE, pre, 0, MPI_COMM_WORLD, &requestRecPre);
			MPI_Irecv(bufPost, N + 1, MPI_DOUBLE, post, 0, MPI_COMM_WORLD, &requestRecPost);
			MPI_Isend(firstLine, N + 1, MPI_DOUBLE, pre, 0, MPI_COMM_WORLD, &requestSendPre);
			MPI_Isend(lastLine, N + 1, MPI_DOUBLE, post, 0, MPI_COMM_WORLD, &requestSendPost);

			MPI_Wait(&requestRecPre, &statusRecPre);
			MPI_Wait(&requestRecPost, &statusRecPost);

			matrixComplete[0] = bufPre;
			matrixComplete[lines + 1] = bufPost;
			for(int x = 1; x <= lines; x++)
			{
				matrixComplete[x] = Matrix_In[x - 1];
			}

			MPI_Wait(&requestSendPre, &statusSendPre);
			MPI_Wait(&requestSendPost, &statusSendPost);
		}

		maxresiduum = 0;

		/* over all rows */
		//for (i = 1; i < N; i++)
		for(i = 1; i < (linesComplete - 1); i++)
		{
			double fpisin_i = 0.0;

			if (options->inf_func == FUNC_FPISIN)
			{
				//da die Schleife nicht mehr über die gesamte Matrix geht, muss i angepasst werden;
				//für alle Prozesse außer den ersten muss der globale Start draufgerechnet werden
				double adaptedI = rank == 0 ? (double) i : (double)(i - 1 + data->globalStart);
				//fpisin_i = fpisin * sin(pih * (double)i);
				fpisin_i = fpisin * sin(pih * adaptedI);
			}

			/* over all columns */
			for (j = 1; j < N; j++)
			{
				star = 0.25 * (matrixComplete[i-1][j] + matrixComplete[i][j-1] + matrixComplete[i][j+1] + matrixComplete[i+1][j]);

				if (options->inf_func == FUNC_FPISIN)
				{
					star += fpisin_i * sin(pih * (double)j);
				}

				if (options->termination == TERM_PREC || term_iteration == 1)
				{
					residuum = matrixComplete[i][j] - star;
					residuum = (residuum < 0) ? -residuum : residuum;
					maxresiduum = (residuum < maxresiduum) ? maxresiduum : residuum;
				}

				//Weil Matrix_Out nur die ursprüngliche Zeilenanzahl ohne die empfangenen
				//hat, müssen hier für alle Ränge != 0 der erste Index um 1 verringert werden
				if(rank == 0)
				{
					Matrix_Out[i][j] = star;
				}
				else
				{
					Matrix_Out[i - 1][j] = star;
				}
			}
		}

		//maxresiduum wird nun reduziert und anschließend an alle gesendet
		double reducedMaxresiduum;
		MPI_Reduce(&maxresiduum, &reducedMaxresiduum, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);

		if(rank == 0)
		{
			maxresiduum = reducedMaxresiduum;
		}
		MPI_Bcast(&maxresiduum, 1, MPI_DOUBLE, 0, MPI_COMM_WORLD);

		results->stat_iteration++;
		results->stat_precision = maxresiduum;

		/* exchange m1 and m2 */
		i = m1;
		m1 = m2;
		m2 = i;

		/* check for stopping calculation depending on termination method */
		if (options->termination == TERM_PREC)
		{
			if (maxresiduum < options->term_precision)
			{
				term_iteration = 0;
			}
		}
		else if (options->termination == TERM_ITER)
		{
			term_iteration--;
		}
	}

	results->m = m2;

	free(bufPre);
	free(bufPost);
	free(matrixComplete);
}

/* ************************************************************************ */
/* calculate: solves the equation                                           */
/* ************************************************************************ */
static
void
calculate (struct calculation_arguments const* arguments, struct calculation_results* results, struct options const* options)
{
	int i, j;                                   /* local variables for loops */
	int m1, m2;                                 /* used as indices for old and new matrices */
	double star;                                /* four times center value minus 4 neigh.b values */
	double residuum;                            /* residuum of current iteration */
	double maxresiduum;                         /* maximum residuum value of a slave in iteration */

	int const N = arguments->N;
	double const h = arguments->h;

	double pih = 0.0;
	double fpisin = 0.0;

	int term_iteration = options->term_iteration;

	/* initialize m1 and m2 depending on algorithm */
	if (options->method == METH_JACOBI)
	{
		m1 = 0;
		m2 = 1;
	}
	else
	{
		m1 = 0;
		m2 = 0;
	}

	if (options->inf_func == FUNC_FPISIN)
	{
		pih = PI * h;
		fpisin = 0.25 * TWO_PI_SQUARE * h * h;
	}

	while (term_iteration > 0)
	{
		double** Matrix_Out = arguments->Matrix[m1];
		double** Matrix_In  = arguments->Matrix[m2];

		maxresiduum = 0;

		/* over all rows */
		for (i = 1; i < N; i++)
		{
			double fpisin_i = 0.0;

			if (options->inf_func == FUNC_FPISIN)
			{
				fpisin_i = fpisin * sin(pih * (double)i);
			}

			/* over all columns */
			for (j = 1; j < N; j++)
			{
				star = 0.25 * (Matrix_In[i-1][j] + Matrix_In[i][j-1] + Matrix_In[i][j+1] + Matrix_In[i+1][j]);

				if (options->inf_func == FUNC_FPISIN)
				{
					star += fpisin_i * sin(pih * (double)j);
				}

				if (options->termination == TERM_PREC || term_iteration == 1)
				{
					residuum = Matrix_In[i][j] - star;
					residuum = (residuum < 0) ? -residuum : residuum;
					maxresiduum = (residuum < maxresiduum) ? maxresiduum : residuum;
				}

				Matrix_Out[i][j] = star;
			}
		}

		results->stat_iteration++;
		results->stat_precision = maxresiduum;

		/* exchange m1 and m2 */
		i = m1;
		m1 = m2;
		m2 = i;

		/* check for stopping calculation depending on termination method */
		if (options->termination == TERM_PREC)
		{
			if (maxresiduum < options->term_precision)
			{
				term_iteration = 0;
			}
		}
		else if (options->termination == TERM_ITER)
		{
			term_iteration--;
		}
	}

	results->m = m2;
}

/* ************************************************************************ */
/*  displayStatistics: displays some statistics about the calculation       */
/* ************************************************************************ */
static
void
displayStatistics (struct calculation_arguments const* arguments, struct calculation_results const* results, struct options const* options)
{
	int N = arguments->N;
	double time = (comp_time.tv_sec - start_time.tv_sec) + (comp_time.tv_usec - start_time.tv_usec) * 1e-6;

	printf("Berechnungszeit:    %f s \n", time);
	printf("Speicherbedarf:     %f MiB\n", (N + 1) * (N + 1) * sizeof(double) * arguments->num_matrices / 1024.0 / 1024.0);
	printf("Berechnungsmethode: ");

	if (options->method == METH_GAUSS_SEIDEL)
	{
		printf("Gauß-Seidel");
	}
	else if (options->method == METH_JACOBI)
	{
		printf("Jacobi");
	}

	printf("\n");
	printf("Interlines:         %" PRIu64 "\n",options->interlines);
	printf("Stoerfunktion:      ");

	if (options->inf_func == FUNC_F0)
	{
		printf("f(x,y) = 0");
	}
	else if (options->inf_func == FUNC_FPISIN)
	{
		printf("f(x,y) = 2pi^2*sin(pi*x)sin(pi*y)");
	}

	printf("\n");
	printf("Terminierung:       ");

	if (options->termination == TERM_PREC)
	{
		printf("Hinreichende Genaugkeit");
	}
	else if (options->termination == TERM_ITER)
	{
		printf("Anzahl der Iterationen");
	}

	printf("\n");
	printf("Anzahl Iterationen: %" PRIu64 "\n", results->stat_iteration);
	printf("Norm des Fehlers:   %e\n", results->stat_precision);
	printf("\n");
}

/****************************************************************************/
/** Beschreibung der Funktion displayMatrix:                               **/
/**                                                                        **/
/** Die Funktion displayMatrix gibt eine Matrix                            **/
/** in einer ubersichtlichen Art und Weise auf die Standardausgabe aus.   **/
/**                                                                        **/
/** Die Ubersichtlichkeit wird erreicht, indem nur ein Teil der Matrix    **/
/** ausgegeben wird. Aus der Matrix werden die Randzeilen/-spalten sowie   **/
/** sieben Zwischenzeilen ausgegeben.                                      **/
/****************************************************************************/
static
void
displayMatrix (struct calculation_arguments* arguments, struct calculation_results* results, struct options* options)
{
	int x, y;

	double** Matrix = arguments->Matrix[results->m];

	int const interlines = options->interlines;

	printf("Matrix:\n");

	for (y = 0; y < 9; y++)
	{
		for (x = 0; x < 9; x++)
		{
			printf ("%7.4f", Matrix[y * (interlines + 1)][x * (interlines + 1)]);
		}

		printf ("\n");
	}

	fflush (stdout);
}

static
void
displayMatrixMPI(struct calculation_arguments* arguments, struct calculation_results* results, struct options* options, struct thread_data* data)
{
	int const elements = 8 * options->interlines + 9;

  	int x, y;
  	double** Matrix = arguments->Matrix[results->m];
  	MPI_Status status;

	int rank = data->rank;
	int from = data->globalStart;
	int to = data->globalEnd;

  	/* first line belongs to rank 0 */
 	 //if (rank == 0)
  	//  from--;

  	/* last line belongs to rank size - 1 */
  	//if (rank + 1 == size)
  	//  to++;

  	if (rank == 0)
    	printf("Matrix:\n");

  	for (y = 0; y < 9; y++)
  	{
    		int line = y * (options->interlines + 1);

    		if (rank == 0)
    		{
      			/* check whether this line belongs to rank 0 */
      			if (line > to)
      			{
        			/* use the tag to receive the lines in the correct order
         			* the line is stored in Matrix[0], because we do not need it anymore */
        			MPI_Recv(Matrix[0], elements, MPI_DOUBLE, MPI_ANY_SOURCE, 42 + y, MPI_COMM_WORLD, &status);
      			}
    		}
    		else
    		{
      			if (line >= from && line <= to)
      			{
        			/* if the line belongs to this process, send it to rank 0
         			* (line - from + 1) is used to calculate the correct local address */
        			//MPI_Send(Matrix[line - from + 1], elements, MPI_DOUBLE, 0, 42 + y, MPI_COMM_WORLD);
				MPI_Send(Matrix[line - from ], elements, MPI_DOUBLE, 0, 42 + y, MPI_COMM_WORLD);
      			}
    		}

   		if (rank == 0)
    		{
      			for (x = 0; x < 9; x++)
      			{
        			int col = x * (options->interlines + 1);

        			if (line >= from && line <= to)
        			{
          				/* this line belongs to rank 0 */
				        printf("%7.4f", Matrix[line][col]);
        			}
        			else
        			{
          				/* this line belongs to another rank and was received above */
          				printf("%7.4f", Matrix[0][col]);
        			}
      			}

      			printf("\n");
    		}
  	}

	fflush(stdout);
}

/* ************************************************************************ */
/*  main                                                                    */
/* ************************************************************************ */
int
main (int argc, char** argv)
{
	MPI_Init(&argc, &argv);
	int rank, world_size;
	MPI_Comm_size(MPI_COMM_WORLD, &world_size);
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);

	struct thread_data data;
	struct options options;
	struct calculation_arguments arguments;
	struct calculation_results results;

	data.rank = rank;
	data.world_size = world_size;

	askParams(&options, argc, argv, &data);

	//Wenn Gauss Seidel nach Präzision und mit mehr als einem Prozess berechnet werden soll,
	//starte den Verwaltungsprozess und verringere world_size, damit Datenaufteilung noch passt
	if(options.method == METH_GAUSS_SEIDEL && options.termination == TERM_PREC && world_size != 1)
	{
		if(world_size == 2 && rank == 0)
		{
			printf("Gauss Seidel funktioniert nur für 1 oder mehr als 2 Prozesse\n");
			exit(1);
		}

		data.world_size = data.world_size - 1;

		if(rank == data.world_size)
		{
			managePrecision(&options, &data);
		}
	}


	initVariables(&arguments, &results, &options, &data);
	allocateMatrices(&arguments, &data);
	initMatrices(&arguments, &options, &data);

	if(rank == 0)
	{
		gettimeofday(&start_time, NULL);
	}

	//wenn es nur einen Prozess gibt, wird die normale calculate-Funktion gerufen
	if(world_size == 1)
	{
		calculate(&arguments, &results, &options);
	}
	else if(options.method == METH_GAUSS_SEIDEL)
	{
		calculateGaussMPI(&arguments, &results, &options, &data);
	}
	else
	{
		calculateJacobiMPI(&arguments, &results, &options, &data);
	}

	if(rank == 0)
	{
		gettimeofday(&comp_time, NULL);
		displayStatistics(&arguments, &results, &options);
	}

	//wenn es nur einen Prozess gibt, kann die normale displayMatrix-Funktion genutzt werden
	if(world_size == 1)
	{
		displayMatrix(&arguments, &results, &options);
	}
	else
	{
		displayMatrixMPI(&arguments, &results, &options, &data);
	}

	freeMatrices(&arguments);

	MPI_Barrier(MPI_COMM_WORLD);
	MPI_Finalize();

	return 0;
}
