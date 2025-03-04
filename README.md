# DFS

A simple distributed filesystem.
#TODO: Add usecase of the project here

## Installation

### Requirements

- CMake >= 2.28.0
- gcc
- git
- Ninja >= 1.3 (optional)

### Build & run

Only support Linux-based OS.

*Setting up the environment*:
- If you did not clone the repo recursively, run:
```bash
git clone git@github.com:duc0905/DFS.git --recurse-submodules
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
where `<target>` is one of the targets above
