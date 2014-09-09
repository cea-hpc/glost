#!/bin/bash

../glost_launch test_sigusr1.list &
pid=$!
sleep 5
kill -s SIGUSR1 $pid
