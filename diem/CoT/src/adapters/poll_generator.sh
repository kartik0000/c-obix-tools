#!/bin/sh

if [ $# -lt 2 ]; then
	echo "Wrong number input arguments!"
	echo "Usage:"
	echo "    poll_generator.sh proc_num arg_list"
	echo "where: "
	echo "    proc_num - Number of poll generator applications to"
	echo "               be executed in parallel;"
	echo "    arg_list - Arguments which should be passed to each"
	echo "               instance of poll_generator."
	exit 1
fi

PROC_COUNT=$1

# Generate execution string 
COMMAND="./poll_generator"

shift
for ARG in $*
do
	COMMAND="$COMMAND $ARG" # append arguments 
done

echo "Now $PROC_COUNT following processes will be started in parallel:"
echo "  $COMMAND"
echo "They may generate a lot of output to the console and tell that they"
echo "can be stopped by pressing Ctrl+C. Don't believe them! Just press Enter"
echo "when you really want to stop everything."  
echo ""
echo "If you are ready, press Enter to continue.."

read INPUT

# launch PROC_COUNT parallel processes
for i in $(seq 1 1 $PROC_COUNT); do
	$COMMAND <&- &
	echo "$i: $COMMAND"	
done

read INPUT

echo "\n\n\n\n"
echo "Terminating all processes!!!!"
killall -s SIGINT -r '(rt-)?poll_generator'