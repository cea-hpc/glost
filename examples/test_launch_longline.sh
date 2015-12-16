#!/bin/bash
echo -n "echo '" > test_launch_longline.list
printf "%s" {0..6000} >> test_launch_longline.list
echo "'" >> test_launch_longline.list

cat test_launch_longline.list >  test_launch_longline.list2
cat test_launch_longline.list >> test_launch_longline.list2
cat test_launch_longline.list >> test_launch_longline.list2

#mpirun -np 1 ../glost_launch test_launch_longline.list
#mpirun -np 1 ../glost_launch -l 50000 test_launch_longline.list &> test_launch_longline.log
mpirun -np 3 ../glost_launch -l 50000 test_launch_longline.list2 &> test_launch_longline.log2
