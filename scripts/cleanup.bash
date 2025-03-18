#!/bin/bash

# Clean up the data on agents and CMMU
mapfile -t hosts < hosts.txt

for host in ${hosts[@]}
do
	# Kill everything
	ssh "${host}" "~/DFS/scripts/cleanup-local.bash"
done
