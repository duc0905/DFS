import argparse
import requests
import numpy as np
from common import bench_single, bench_batch


def write(addr: str, port: int, file: str):
    # print(f"addr: {addr}:{port} | file: {file}")
    res = requests.post(
        f"http://{addr}:{port}/write", files={"benchmark_write": open(file, "rb")}
    )
    return


if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        prog="benchmark_write", description="Benchmark write operations for DFS"
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
        "-f", "--file", help="The file to be used", nargs=1, required=True
    )

    parser.add_argument("-n", help="Number of reads", default=1, nargs=1)

    args = parser.parse_args()

    host = args.host
    port = args.port
    file = args.file

    print(f"Host: {host}, port: {port}, file: {file}")

    try:
        print("Sequential benchmarking:")
        times = bench_single(10, write, args=(host, port, file[0]))
        times = np.array(times) / 10e9
        print(f"Average: {np.mean(times)}s")
        print(f"Median: {np.median(times)}s")
        print(f"Variance: {np.var(times)}s")
        print("============================")
    except Exception as e:
        print(f"Error while bench: {e}")

    try:
        print("Batch benchmarking:")
        time = bench_batch(20, write, args=(host, port, file[0]))
        # times = np.array(times) / 10e9
        print(f"Time: {time / 10e9}")
        print("============================")
    except Exception as e:
        print(f"Error while bench: {e}")
