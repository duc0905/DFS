#!/bin/python3

import argparse

# Maximum length of the string used to create file
MAX_LEN = 1024 * 1024 * 16 # 16MB

def create_file(filename: str, filesize: int, pattern: str = "abcd") -> None:
    f = open(filename, "w")

    n = len(pattern)

    # How many times should we replicate the pattern
    N = min(filesize // n, MAX_LEN // n)

    _size = 0
    P = pattern * N

    print("Using string with length:", N * n)

    while _size + (N * n) <= filesize:
        f.write(P)
        _size += (N * n)

    f.write(P[:(filesize - _size)])
    f.close()

if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        prog="benchmark_write", description="Benchmark write operations for DFS"
    )

    parser.add_argument(
        "filename",
        help="Name of the file",
        type=str,
        nargs="?")

    parser.add_argument(
        "size",
        help="Size of the file in bytes",
        type=int,
        nargs="?")

    parser.add_argument(
        "-p",
        "--pattern",
        help="Repeating pattern of the content of the file",
        default="abcd",
        type=str,
        nargs="?")

    args = parser.parse_args()

    filename: str = args.filename
    size: int = args.size
    pattern: str = args.pattern

    print(f"Filename: {filename}")
    print(f"File size: {size}B")
    print(f"Pattern: {pattern}")
    create_file(filename, size, pattern)

