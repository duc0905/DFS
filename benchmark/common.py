from typing import Callable
from threading import Thread
import time


def bench_batch(n: int, func: Callable, args: tuple) -> int:
    """
    Benchmark the time it takes to finish n jobs in parallel
    """
    # Prepare the jobs
    threads = [Thread(target=func, args=args) for _ in range(n)]

    start = time.perf_counter_ns()

    for t in threads:
        t.start()

    for t in threads:
        t.join()

    end = time.perf_counter_ns()

    return end - start


def bench_single(n: int, func: Callable, args: tuple = ()) -> list[int]:
    """
    Benchmark the time it takes on average to finish 1 job in n trials
    """
    times: list[int] = []

    for _ in range(n):
        start = time.perf_counter_ns()
        func(*args)
        end = time.perf_counter_ns()
        times.append(end - start)

    return times


def bench_mixed(n: int, fs: list[tuple[Callable, tuple]]) -> list[int]:
    """
    Benchmark a list of user-defined functions to be benchmarked, n trials
    """

    times: list[int] = []

    for _ in range(n):
        threads = [Thread(target=f[0], args=f[1]) for f in fs]

        start = time.thread_time_ns()

        for t in threads:
            t.run()

        for t in threads:
            t.join()

        end = time.thread_time_ns()
        times.append(end - start)

    return times
