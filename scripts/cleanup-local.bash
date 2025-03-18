#!/bin/bash

# Kill the services
pkill "(CMMU)|(Agent)"

# Delete Agent files
rm /virtual/leduc7/dfs-data/*

# Delete CMMU files
rm /virtual/leduc7/dfs-meta/*
