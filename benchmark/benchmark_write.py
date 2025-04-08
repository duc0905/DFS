import argparse
import requests
import numpy as np
import json
from common import bench_single, bench_batch

def write_v2(addr: str, port: int, file: str, filename: str):
    # print(f"addr: {addr}:{port} | file: {file}")
    try:
        res = requests.post(
            f"http://{addr}:{port}/write/v2", files={filename: open(file, "rb")}
        )

        if res.status_code != 201:
            print("[Warning]: response code: ", res.status_code)
    except:
        exit(1)

    return

def write_v1(addr: str, port: int, file: str, filename: str):
    # print(f"addr: {addr}:{port} | file: {file}")
    res = requests.post(
        f"http://{addr}:{port}/write", files={filename: open(file, "rb")}
    )

    if res.status_code != 201:
        print("[Warning]: response code: ", res.status_code)
        exit(1)

    return

files = [
    "files/10KB.txt",   # Relatively small
    "files/100KB.txt",   # Relatively small
    "files/1MB.txt",    # 1 chunk
    "files/10MB.txt",    # 3 chunk
    "files/100MB.txt",  # Multiple chunks
    # "files/1GB.txt",    # Relatively big
]

ns = [1, 2, 4, 8, 16, 24, 32]


if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        prog="benchmark_write", description="Benchmark write operations for DFS"
    )

    parser.add_argument(
        "-H",
        "--host",
        help="The IP address of the entrypoint Agent",
        default="localhost",
        nargs="?",
    )

    parser.add_argument(
        "-P",
        "--port",
        help="The port the entrypoint Agent is listening on",
        default=1234,
        nargs="?",
    )

    parser.add_argument(
        "-f", "--file", help="The files to be used", nargs="*"
    )

    parser.add_argument("-o", help="Output file (JSON)", default=None, nargs="?")

    args = parser.parse_args()

    host = args.host
    port = args.port

    if args.file:
        files = args.file

    results = []

    for n in ns:
        for file in files:
            print(f"Benchmarking with file: {file}")
            res = {
                "filename": file,
                "n": n,
                "sequential": {
                    "raw": [],
                    "mean": 0,
                    "variance": 0,
                    "median": 0
                },
                "batch": {
                    "raw": 0,
                    "average": 0
                }
            }

            try:
                print(f"Sequential {n}:")
                times = bench_single(n, write_v2, args=(host, port, file, file))
                res["sequential"]["raw"] = times
                times = np.array(times)

                res["sequential"]["mean"] = np.mean(times)
                res["sequential"]["median"] = np.median(times)
                res["sequential"]["variance"] = np.var(times)

                print(f"Average: {np.mean(times)}s")
                print(f"Median: {np.median(times)}s")
                print(f"Variance: {np.var(times)}s^2")
                print("============================")
            except Exception as e:
                print(f"Error while bench: {e}")

            try:
                print(f"Batch {n}:")
                time = bench_batch(n, funcs=[write_v2 for _ in range(n)], argss=[(host, port, file, f"write_{i}_{file}") for i in range(n)])
                res["batch"]["raw"] = time
                res["batch"]["average"] = time / n

                print(f"Time: {time}s")
                print(f"Avg Time: {time / n}s")
                print("============================")
            except Exception as e:
                print(f"Error while bench: {e}")

            results.append(res)

    if args.o:
        with open(args.o, "w") as outfile:
            json.dump(results, outfile)

