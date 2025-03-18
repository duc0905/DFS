#!/bin/python3

import argparse
import requests
import numpy as np
import json
from common import bench_single, bench_batch


def read(addr: str, port: int, file: str):
    # print(f"addr: {addr}:{port} | file: {file}")
    res = requests.get(
        f"http://{addr}:{port}/read?filepath={file}"
    )

    if res.status_code > 299:
        print("[Warning]: response code not 200")
        print("status: ", res.status_code)
        print("file: ", file)

    return

files = [
    "files/10KB.txt",   # Relatively small
    "files/1MB.txt",    # 1 chunk
    # "files/100MB.txt",  # Multiple chunks
    # "files/1GB.txt",    # Relatively big
]


if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        prog="benchmark_read", description="Benchmark read operations for DFS"
    )

    parser.add_argument(
        "-H",
        "--host",
        help="The IP address of the entrypoint Agent",
        default="localhost",
        nargs=1,
    )

    parser.add_argument(
        "-P",
        "--port",
        help="The port the entrypoint Agent is listening on",
        default=1234,
        nargs=1,
    )

    parser.add_argument(
        "-f", "--file", help="The files to be used", nargs="*"
    )

    parser.add_argument("-n", help="Number of reads", default=1, type=int)

    parser.add_argument("-o", help="Output file (JSON)", default=None, nargs="?")

    args = parser.parse_args()

    host = args.host
    port = args.port
    n = args.n

    if args.file:
        files = args.file

    print("n: ", n)
    print("files: ", files)

    results = [["Filename", "N", "Sequential", "Batch"]]

    for file in files:
        print(f"Benchmarking with file: {file}")
        res = [file, n, [], 0]
        try:
            print(f"Sequential {n}:")
            times = bench_single(n, read, args=(host, port, file))
            res[2] = times
            times = np.array(times)
            print(f"Average: {np.mean(times)}s")
            print(f"Median: {np.median(times)}s")
            print(f"Variance: {np.var(times)}s^2")
            print("============================")
        except Exception as e:
            print(f"Error while bench: {e}")

        try:
            print(f"Batch {n}:")
            time = bench_batch(n, read, args=(host, port, file))
            res[3] = time
            # times = np.array(times) / 10e9
            print(f"Time: {time}s")
            print("============================")
        except Exception as e:
            print(f"Error while bench: {e}")

        results.append(res)

    if args.o:
        with open(args.o, "w") as outfile:
            json.dump(results, outfile)

