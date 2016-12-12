#!/bin/sh

echo -e "\n\n******************************** test case 4 ********************************\n"

echo -e "\tcreating producer and consumer processes..\n"
./producer/producer 50 BLACK &
PID1=$!
./producer/producer 50 RED &
PID2=$!
./consumer/consumer 100 A &
PID3=$!
wait "$PID1" "$PID2" "$PID3"

