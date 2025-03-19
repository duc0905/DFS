from common import plot_raw, plot_boxes, plot_stats
import json

if __name__ == "__main__":
    f = open("./test.json", "r")
    results = json.load(f)

    # plot_raw(results, "Raw data 1 CMMU 1 Agent", "raw.png")
    #
    # plot_boxes(results, "Sequential time boxes 1 CMMU 1 Agent", "box.png")
    #
    plot_stats(results, "Sequential vs. Batch performace 1 CMMU 1 Agent", "stat.png")


