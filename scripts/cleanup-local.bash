#!/bin/bash

# Kill the services
pkill "(CMMU)|(Agent)"

# Delete Agent files
rm $1/dfs-data/*

# Delete CMMU files
rm $1/dfs-meta/*
