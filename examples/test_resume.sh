#!/bin/bash

for i in $(seq 1 3); do
    echo "launch number $i"
    jobid=`ccc_msub test_resume.msub | cut -d ' ' -f4` # launching
    sleep 30s                                            # waiting
    ccc_mdel -s SIGUSR1 $jobid                           # killing properly
done