# DFS

A simple distributed filesystem.
#TODO: Add usecase of the project here

## Installation

### Requirements

- CMake >= 2.28.0
- Ninja >= 1.3
- gcc
- git

### Build & run

Only support Linux-based OS.

*Setting up the environment*:
- If you did not clone the repo recursively, run:
```bash
# TODO
```
to clone vcpkg and other dependencies (if there is).

Build everything:
```
cmake --preset default
cmake --build --preset default
```

List of targets:
- "src/CMMU": Centralized metadata management unit
- "src/Agent": Agent that will be run on each node
- #TODO

Running targets:
```
./build/default/<target>
```
where `<target` is one of the targets above
