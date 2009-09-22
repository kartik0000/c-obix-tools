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
	./test_adapter $1 /obix/memtest$i $4 $5 <&- >> /dev/null 2>&1 || \
		echo "$i is terminated!" >> stop.log &
	echo "Adapter N $i is started..."
	echo "Adapter N $i is started..." >> mem_test.log
done

# launch [prd] more processes. Each process in started once in 2 seconds.
echo "Starting processes with delay.."
i=$(( $2 + 1 ))
TOTAL_PRC=$(( $2 + $3 ))
while [ $i -le $TOTAL_PRC ]; do
	sleep 2
	# check that everything is OK so far
	if [ -f stop.log ]; then
		echo "Some adapter has been terminated! Stop test."
		echo "Some adapter has been terminated! Stop test." >> mem_test.log
		exit 1
	fi
	
	./test_adapter $1 /obix/memtest$i $4 $5 <&- >> /dev/null 2>&1 || \
		echo "$i is terminated!" >> stop.log &
	echo "Adapter N $i is started..."
	echo "Adapter N $i is started..." >> mem_test.log
	i=$(( $i + 1 ))
done

