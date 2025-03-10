import argparse

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
    "-f", "--file", help="The file to be used", nargs=1, required=True)

parser.add_argument("-n", help="Number of reads", default=1, nargs=1)

parser.print_help()
