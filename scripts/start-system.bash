#!/bin/bash

mapfile -t hosts < hosts.txt

CMMU_HOST=${hosts[0]}
AGENT_HOSTS=${hosts[@]:1:${#hosts[@]}}

echo "CMMU_HOST: $CMMU_HOST"
echo "AGENT_HOSTS: $AGENT_HOSTS"

ssh ${CMMU_HOST} "~/DFS/scripts/cmmu.bash $1" > /virtual/leduc7/dfs-logs/${CMMU_HOST}.cmmu.log &

sleep 1s

for host in ${AGENT_HOSTS[@]}
do
	echo "Running agent on host $host"
	ssh ${host} "~/DFS/scripts/agent.bash ${CMMU_HOST} 4321" > "/virtual/leduc7/dfs-logs/${host}.agent.log" &
done
