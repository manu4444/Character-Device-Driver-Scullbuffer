#!/bin/sh

echo -e "\n\n******************************** test case 2 ********************************\n"

echo -e "\tcreating producer and consumer processes..\n"
./producer/producer 50 BLACK &
PID1=$!
./consumer/consumer 10 A &
PID2=$!
wait "$PID1" "$PID2"
