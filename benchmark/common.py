from typing import Callable
from threading import Thread
import matplotlib.pyplot as plt
import numpy as np
import time

def bench_batch(n: int, funcs: list[Callable], argss: list[tuple]) -> float:
    """
    Benchmark the time it takes to finish n jobs in parallel
    """
    # Prepare the jobs
    threads = [Thread(target=func, args=args) for [func, args] in zip(funcs, argss)]

    start = time.perf_counter()

    for t in threads:
        t.start()

    print("Started all threads")

    for t in threads:
        t.join()

    end = time.perf_counter()

    return end - start


def bench_single(n: int, func: Callable, args: tuple = ()) -> list[float]:
    """
    Benchmark the time it takes on average to finish 1 job in n trials
    """
    times: list[float] = []

    for _ in range(n):
        start = time.perf_counter()
        func(*args)
        end = time.perf_counter()
        times.append(end - start)

    return times


def bench_mixed(n: int, fs: list[tuple[Callable, tuple]]) -> list[float]:
    """
    Benchmark a list of user-defined functions to be benchmarked, n trials
    """

    times: list[float] = []

    for _ in range(n):
        threads = [Thread(target=f[0], args=f[1]) for f in fs]

        start = time.perf_counter()

        for t in threads:
            t.run()

        for t in threads:
            t.join()

        end = time.perf_counter()
        times.append(end - start)

    return times

files = [
    "files/10KB.txt",   # Relatively small
    "files/100KB.txt",   # Relatively small
    "files/1MB.txt",    # 1 chunk
    "files/10MB.txt",    # 3 chunk
    "files/100MB.txt",  # Multiple chunks
    # "files/1GB.txt",    # Relatively big
]

def plot_raw(results, title, outfile):

    X2 = []
    Y2 = []
    C2 = []

    fig = plt.figure()
    ax = plt.subplot(111)

    colors = plt.cm.jet(np.linspace(0,1,len(files)))

    for i, file in enumerate(files):
        X = []
        Y = []

        for res in results:
            if res["filename"] != file: continue
            
            n = res["n"]
            y = np.log(res["sequential"]["raw"])
            x = [n] * len(y)
            X.extend(x)
            Y.extend(y)

        ax.scatter(X, Y, s=40, color=colors[i], label=f"{file[6:-4]}")

    for res in results:
        f_index = files.index(res["filename"])
        color = colors[f_index]

        X2.append(res["n"])
        Y2.append(np.log(res["batch"]["raw"]))
        C2.append(color)

    ax.scatter(X2, Y2, s=80, c=C2, marker='s', label="Batch")

    ax.legend(loc='center left', fancybox=True, bbox_to_anchor=(1.0, 0.5))
    plt.subplots_adjust(right=0.8)

    ax.set_title(title)
    ax.set_xlabel("n")
    ax.set_ylabel("log(time(s))")
    fig.savefig(outfile)

def plot_boxes(results, title, outfile):
    D = [[] for _ in files] # (X x F), X = sum(n)

    for res in results:
        f_index = files.index(res["filename"])
        D[f_index].extend(np.log(res["sequential"]["raw"]))
    D = np.array(D)

    D = D.T

    fig = plt.figure()
    ax = plt.subplot(111)
    ax.boxplot(D, tick_labels=[file[6:-4] for file in files])

    ax.set_title(title)
    ax.set_xlabel("File size")
    ax.set_ylabel("log(time(s))")
    fig.savefig(outfile)


def plot_stats(results, title, outfile):
    fig = plt.figure()
    ax = plt.subplot(111)
    colors = plt.cm.jet(np.linspace(0,1,len(files)))

    for i, file in enumerate(files):
        seq_line = []
        bat_line = []
        x = []
        for res in results:
            if res["filename"] != file: continue
            seq_mean = np.mean(np.log(res["sequential"]["raw"]))
            bat_mean = np.log(res["batch"]["raw"] / res["n"])

            seq_line.append(seq_mean)
            bat_line.append(bat_mean)
            x.append(res["n"])
        ax.plot(x, seq_line, color=colors[i], label=f"Seq {file[6:-4]}")
        ax.plot(x, bat_line, color=colors[i], linestyle='dashed', label=f"Bat {file[6:-4]}")

    ax.legend()

    ax.legend(loc='center left', fancybox=True, bbox_to_anchor=(1.0, 0.5))
    plt.subplots_adjust(right=0.8)

    ax.set_title(title)
    ax.set_xlabel("n")
    ax.set_ylabel("log(time(s))")
    fig.savefig(outfile)
