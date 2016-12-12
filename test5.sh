#!/bin/sh

echo -e "\n\n******************************** test case 5 ********************************\n"

echo -e "\tcreating producer and consumer processes..\n"
./producer/producer 50 BLACK &
PID1=$!
./consumer/consumer 25 A &
PID2=$!
./consumer/consumer 25 B &
PID3=$!
wait "$PID1" "$PID2" "$PID3"

