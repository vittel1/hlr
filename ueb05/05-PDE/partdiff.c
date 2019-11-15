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
#include <pthread.h>

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

struct thread_data{
	struct options const* options;
	struct calculation_arguments const* arguments;
	struct calculation_results* results;
	int step_size; /*Anzahl der Iterationen der äußeren for-Schleife eines Threads*/
	int start;	/*Startindex der äußeren for-Schleife eines Threads*/
	double pih;
	double fpisin;
	int tid; /*einzigartige Thread ID*/
};

/* ************************************************************************ */
/* Global variables                                                         */
/* ************************************************************************ */

double maxresiduum;                         /* maximum residuum value of a slave in iteration */
int term_iteration;

pthread_mutex_t mutex_res;	/*Mutex für maxresiduum*/

/*Barriers zum Synchronisieren der Threads*/
pthread_barrier_t barrFirst;
pthread_barrier_t barrLast;
pthread_barrier_t barrMid;

/*m1 und m2 können global gesetzt werden und müssen auch nicht verändert werden,
da diese Variante des Programms für Gauß-Seidel eh nicht mehr funktioniert*/
int m1 = 0;
int m2 = 1;

/* time measurement variables */
struct timeval start_time;       /* time when program started                      */
struct timeval comp_time;        /* time when calculation completed                */


/* ************************************************************************ */
/* initVariables: Initializes some global variables                         */
/* ************************************************************************ */
static
void
initVariables (struct calculation_arguments* arguments, struct calculation_results* results, struct options const* options)
{
	arguments->N = (options->interlines * 8) + 9 - 1;
	arguments->num_matrices = (options->method == METH_JACOBI) ? 2 : 1;
	arguments->h = 1.0 / arguments->N;

	results->m = 0;
	results->stat_iteration = 0;
	results->stat_precision = 0;
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
static
void
allocateMatrices (struct calculation_arguments* arguments)
{
	uint64_t i, j;

	uint64_t const N = arguments->N;

	arguments->M = allocateMemory(arguments->num_matrices * (N + 1) * (N + 1) * sizeof(double));
	arguments->Matrix = allocateMemory(arguments->num_matrices * sizeof(double**));

	for (i = 0; i < arguments->num_matrices; i++)
	{
		arguments->Matrix[i] = allocateMemory((N + 1) * sizeof(double*));

		for (j = 0; j <= N; j++)
		{
			arguments->Matrix[i][j] = arguments->M + (i * (N + 1) * (N + 1)) + (j * (N + 1));
		}
	}
}

/* ************************************************************************ */
/* initMatrices: Initialize matrix/matrices and some global variables       */
/* ************************************************************************ */
static
void
initMatrices (struct calculation_arguments* arguments, struct options const* options)
{
	uint64_t g, i, j;                                /*  local variables for loops   */

	uint64_t const N = arguments->N;
	double const h = arguments->h;
	double*** Matrix = arguments->Matrix;

	/* initialize matrix/matrices with zeros */
	for (g = 0; g < arguments->num_matrices; g++)
	{
		for (i = 0; i <= N; i++)
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
		for (g = 0; g < arguments->num_matrices; g++)
		{
			for (i = 0; i <= N; i++)
			{
				Matrix[g][i][0] = 1.0 - (h * i);
				Matrix[g][i][N] = h * i;
				Matrix[g][0][i] = 1.0 - (h * i);
				Matrix[g][N][i] = h * i;
			}

			Matrix[g][N][0] = 0.0;
			Matrix[g][0][N] = 0.0;
		}
	}
}

/*Diese Funktion wird von den einzelnen Threads ausgeführt, im Parameter
thread_args werden alle wichtigen Daten wie Thread ID oder Anzahl der Iterationen
übergeben*/
static void *calc_loop(void* thread_args)
{
	//Typkonversion damit vernünftig auf thread_args zugegriffen werden kann
	struct thread_data* data;
	data = (struct thread_data*) thread_args;

	//Initialisierung der lokalen Variablen aus den übergebenen Daten
	int step_size = data->step_size;
	int start = data->start;
	double pih = data->pih;
	double fpisin = data->fpisin;
	int tid = data->tid;

	struct calculation_arguments const* arguments = data->arguments;
	struct calculation_results* results = data->results;
	struct options const* options = data->options;

	int const N = arguments->N;

	double star,residuum;
	int i,j;


	while (term_iteration > 0)
	{
		double** Matrix_Out = arguments->Matrix[m1];
		double** Matrix_In  = arguments->Matrix[m2];

		//maxresiduum muss nur von einem einzigen Thread angepasst werden, also wird tid abgefragt
		if(tid == 0)
		{
			maxresiduum = 0;
		}

		//Barrier zur Vermeidung von Race conditions
		pthread_barrier_wait(&barrFirst);

		/* over all rows */
		for (i = start; i < start + step_size; i++)
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

					//maxresiduum ist global, also muss der Zugriff durch Mutexes geregelt werden
					pthread_mutex_lock(&mutex_res);
					maxresiduum = (residuum < maxresiduum) ? maxresiduum : residuum;
					pthread_mutex_unlock(&mutex_res);
				}

				Matrix_Out[i][j] = star;
			}
		}

		//erneute Synchronisierung, damit Thread mit tid=0 Werte wie stat_precision nicht zu früh setzt
		pthread_barrier_wait(&barrMid);
		//folgender Code muss nur von einem Thread ausgeführt werden
		if(tid == 0)
		{

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
	 //erneutes Synchronisieren (potentiell zu viele Barriers, aber funktioniert)
	 pthread_barrier_wait(&barrLast);
	}
	if(tid == 0)
	{
		results->m = m2;
	}
	//Freigeben der thread_args und Beenden des jeweiligen Threads
	free(data);
	pthread_exit(NULL);
}

/* ************************************************************************ */
/* calculate: solves the equation                                           */
/* ************************************************************************ */
static
void
calculate (struct calculation_arguments const* arguments, struct calculation_results* results, struct options const* options)
{

	int const N = arguments->N;
	double const h = arguments->h;
	term_iteration = options->term_iteration;
	double pih = 0.0;
	double fpisin = 0.0;

	if (options->inf_func == FUNC_FPISIN)
	{
		pih = PI * h;
		fpisin = 0.25 * TWO_PI_SQUARE * h * h;
	}

//Deklaration der benötigten Anzahl Threads und Variablen
	pthread_t threads [options->number];
	int rc;
	void* status;

	//Thread-Attribut genutzt um Threads explizit joinable zu machen
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

	for(size_t i = 0; i < options->number; i++)
	{
		//Aufbau des benötigten Parameters
		struct thread_data *args = malloc(sizeof(struct thread_data));
		args->options = options;
		args->results = results;
		args->arguments = arguments;
		args->pih = pih;
		args->fpisin = fpisin;
		args->step_size = (arguments->N) / (options->number);
		int start = i * args->step_size + 1;

		//damit der letzte Thread in jedem Fall bis N läuft
		if(i == (options->number)-1)
		{
			args->step_size = N - start;
		}

		args->start = start;
		args->tid = i;

		//Threads werden erzeugt, bei Fehler wird das Programm abgebrochen
		rc = pthread_create(&threads[i], &attr, calc_loop, (void*) args);
		if(rc){
			printf("Thread creation failed");
			exit(1);
		}
	}
	pthread_attr_destroy(&attr);

	//Threads werden gejoined, bei Fehler wird das Programm abgebrochen
	for(size_t i = 0; i < options->number; i++)
	{
		rc = pthread_join(threads[i], &status);
		if(rc)
		{
			printf("Thread joining broke");
		}
	}
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
/** in einer "ubersichtlichen Art und Weise auf die Standardausgabe aus.   **/
/**                                                                        **/
/** Die "Ubersichtlichkeit wird erreicht, indem nur ein Teil der Matrix    **/
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

/* ************************************************************************ */
/*  main                                                                    */
/* ************************************************************************ */
int
main (int argc, char** argv)
{
	struct options options;
	struct calculation_arguments arguments;
	struct calculation_results results;

	askParams(&options, argc, argv);

	//Mutex und Barriers müssen initialisiert werden
	pthread_mutex_init(&mutex_res, NULL);
	pthread_barrier_init(&barrFirst, NULL, options.number);
	pthread_barrier_init(&barrLast, NULL, options.number);
	pthread_barrier_init(&barrMid, NULL, options.number);

	initVariables(&arguments, &results, &options);

	allocateMatrices(&arguments);
	initMatrices(&arguments, &options);

	gettimeofday(&start_time, NULL);
	calculate(&arguments, &results, &options);
	gettimeofday(&comp_time, NULL);

	displayStatistics(&arguments, &results, &options);
	displayMatrix(&arguments, &results, &options);

	freeMatrices(&arguments);

	//Mutex und Barriers müssen am Ende wieder freigegeben werden
	pthread_mutex_destroy(&mutex_res);
	pthread_barrier_destroy(&barrFirst);
	pthread_barrier_destroy(&barrLast);
	pthread_barrier_destroy(&barrMid);

	return 0;
}
