#!/bin/bash

mapfile -t hosts < hosts.txt

for host in "${hosts[@]}"
do
	echo "Setting up ${host}"
	ssh ${host} "mkdir -p \"$1/dfs-meta\" \"$1/dfs-data\" \"$1/dfs-logs\""
done
