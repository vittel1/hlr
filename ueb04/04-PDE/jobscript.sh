#!/bin/bash

#SBATCH --job-name=Jacobi
#SBATCH --partition=west
#SBATCH --account=papenfuss
#SBATCH --output=result1Thread.out

srun ./partdiff 1 2 512 2 2 10240

echo "fertig" > job_script.out
