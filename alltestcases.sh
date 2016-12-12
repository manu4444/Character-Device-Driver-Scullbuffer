#!/bin/sh

module="scull"

if [ "$(id -u)" != "0" ]; then
	echo "Sorry, you are not root! Please execute with sudo permissions."
	exit 1
fi

echo -e "\thandling unload, load and setting permissions for scullbuffer device driver..\n"
chmod 755 ./scullbuffer/*

if lsmod | grep "$module" &> /dev/null ; then
    sh ./scullbuffer/scull_unload
fi

sh ./scullbuffer/scull_load
chmod 777 /dev/scull*

echo -e "\nThis will execute all the 5 test cases for the sculldriver..\n"

sleep 1.5

( . ./test1.sh )
echo -e "\ntest-case 1 finished.\n"
sleep 1

( . ./test2.sh )
echo -e "\ntest-case 2 finished.\n"
sleep 1

( . ./test3.sh )
echo -e "\ntest-case 3 finished.\n"
sleep 1

( . ./test4.sh )
echo -e "\ntest-case 4 finished.\n"
sleep 1

( . ./test5.sh )
echo -e "\ntest-case 5 finished.\n"
echo -e "\nAll test-cases are completed.\n"
echo -e "Unloading the scull module."
sh ./scullbuffer/scull_unload

