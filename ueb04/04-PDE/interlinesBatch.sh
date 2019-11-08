#!/bin/bash

#SBATCH --job-name=Interlines
#SBATCH --partition=west
#SBATCH --account=papenfuss

srun ./partdiff 12 2 $1 2 2 3000

