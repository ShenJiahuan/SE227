#!/bin/bash
./stop.sh >/dev/null 2>&1
./stop.sh >/dev/null 2>&1
./stop.sh >/dev/null 2>&1
./stop.sh >/dev/null 2>&1
./stop.sh >/dev/null 2>&1

mkdir ./yfs1 >/dev/null 2>&1
mkdir ./yfs2 >/dev/null 2>&1

./start.sh

client1=./yfs1
client2=./yfs2

test_if_has_mount(){
	mount | grep -q "yfs_client"
	if [ $? -ne 0 ];
	then
			echo "FATAL: Your YFS client has failed to mount its filesystem!"
			exit
	fi;
	yfs_count=$(ps -e | grep -o "yfs_client" | wc -l)
	extent_count=$(ps -e | grep -o "extent_server" | wc -l)

	if [ $yfs_count -ne 2 ];
	then
			echo "error: yfs_client not found (expecting 2)"
			exit
	fi;

	if [ $extent_count -ne 1 ];
	then
			echo "error: extent_server not found"
			exit
	fi;
}
test_if_has_mount

##################################################

# run evil part 1
echo -n "Testing evil part 1... "
timeout 10 ./evil_part1.py $client1
if [ $? -ne 0 ];
then
        echo "Failed evil part 1"
        exit
else

	ps -e | grep -q "yfs_client"
	if [ $? -ne 0 ];
	then
			echo "FATAL: yfs_client DIED!"
			exit
	fi

fi
test_if_has_mount
echo "Passed"

##################################################

# run evil part 2
echo -n "Testing evil part 2... "
timeout 30 ./evil_part2.py $client1 $client2
if [ $? -ne 0 ];
then
        echo "Failed evil part 2"
        exit
else

	ps -e | grep -q "yfs_client"
	if [ $? -ne 0 ];
	then
			echo "FATAL: yfs_client DIED!"
			exit
	fi

fi
test_if_has_mount
echo "Passed"

##################################################

# run evil part 3
echo -n "Testing evil part 3... "
timeout 120 ./evil_part3.py $client1
if [ $? -ne 0 ];
then
        echo "Failed evil part 3"
        exit
else

	ps -e | grep -q "yfs_client"
	if [ $? -ne 0 ];
	then
			echo "FATAL: yfs_client DIED!"
			exit
	fi

fi
test_if_has_mount
echo "Passed"

##################################################

./stop.sh
echo "How could you pass my evil tests?"
echo "You beat me!"
