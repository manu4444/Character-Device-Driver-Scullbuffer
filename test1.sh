#!/bin/sh

echo -e "\n\n******************************** test case 1 ********************************\n"

echo -e "\tcreating producer and consumer processes..\n"
./producer/producer 50 BLACK &
PID1=$!
./consumer/consumer 50 A &
PID2=$!
wait "$PID1" "$PID2"
