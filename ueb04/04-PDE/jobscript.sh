#!/bin/bash

#SBATCH --job-name=Jacobi
#SBATCH --partition=west
#SBATCH --account=papenfuss

srun ./partdiff $1 2 512 2 2 1250

