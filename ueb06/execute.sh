#!/bin/bash

#SBATCH --job-name=Output
#SBATCH --partition=west
#SBATCH --account=urban
#SBATCH --nodes=4
#SBATCH --ntasks-per-node=2
#SBATCH --cpus-per-task=6
#SBATCH --output=timempi.out

#Wird bisher für jeden einzelnen Node ausgeführt, Ausgabe dementsprechend n*Node
srun mpirun -n 4 ./mpi_hello_world
#srun -n 4 ./mpi_hello_world


#liefert das gewünschte Ergebnis, 32 Processe aufgeteilt auf 2 Nodes
#salloc -A urban --nodes=2 --partition=west
#mpirun -n 32 ./mpi_hello_world
