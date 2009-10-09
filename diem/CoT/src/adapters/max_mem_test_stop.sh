#!/bin/sh

echo "Terminating all test adapters if any.."
killall -w -s SIGINT -r '(rt-)?memtest_adapter'

if [ $# = 1 ] && [ $1 = "-c" ]; then
	echo "Cleaning all log files.."
	rm stop.log mem_test.log 
fi