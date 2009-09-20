#!/bin/sh
if [ $# != 5 ]; then
	echo "Wrong number input arguments!"
	echo "Usage: max_mem_test.sh [cfg] [pri] [prd] [memi] [memd]"
	echo "where: "
	echo "    [cfg] - Address to the config file for adapters;"
	echo "    [pri] - Number of processes (adapters) to be started immediately;"
	echo "    [prd] - Number of processes to be started delayed;"
	echo "    [memi] - Amount of additional memory allocated by each adapter immediately;"
	echo "    [memd] - Amount of additional memory allocated by each process delayed;"
	exit 1
fi

# launch [pri] parallel processes
for i in $(seq 1 1 $2); do
	./test_adapter $1 /obix/memtest$i $4 $5 <&- >> log$i.txt 2>&1 || \
		echo "$i is terminated!" >> stop.txt &
	echo Adapter N $i is started...	
done


