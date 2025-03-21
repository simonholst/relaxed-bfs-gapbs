import argparse
import subprocess
import os
import time
from dataclasses import dataclass
from itertools import product
from typing import Callable

DEFAULT_N_SAMPLES = 2
DEFAULT_N_SUBQUEUES = 64
DEFAULT_BATCH_SIZE = 16
DEFAULT_DEBUG = "FALSE"


@dataclass
class Algorithm:
    name: str
    display_name: str
    make_function: Callable
    executable: str
    compilation_flags: list[str]


@dataclass
class Args:
    bfsargs: str
    threads: list[int]
    output_dir: str
    n_samples: list[int]
    n_subqueues: list[int]
    batch_sizes: list[int]
    debug: list[str]
    queues: list[str]


def parse_compilation_flags(kwards):
    flags = ""
    for key, value in kwards.items():
        flags += f"{key}={value} "
    return flags


def make_bfs(queue, **kwargs):
    return f"make bfs QUEUE={queue} " + parse_compilation_flags(kwargs)


def make_rbfs(queue, **kwargs):
    return f"make relax_rbfs QUEUE={queue} " + parse_compilation_flags(kwargs)


def make_rbfs_batching(queue, **kwargs):
    return f"make relax_rbfs_batching QUEUE={queue} " + parse_compilation_flags(kwargs)


def make_rbfs_batching_predeq(queue, **kwargs):
    return f"make relax_rbfs_batching_predeq QUEUE={queue} " + parse_compilation_flags(
        kwargs
    )


def make_rbfs_batching_predeq_depth_thresh(queue, **kwargs):
    return (
        f"make relax_rbfs_batching_predeq_depth_thresh QUEUE={queue} "
        + parse_compilation_flags(kwargs)
    )


ALGORITHMS = [
    Algorithm("DO", "DO", make_bfs, "bfs", []),
    Algorithm("MS", "MS", make_rbfs, "relax_rbfs", ["DEBUG"]),
    Algorithm("FAA", "FAA", make_rbfs, "relax_rbfs", ["DEBUG"]),
    Algorithm("FAA_INT", "FAA_INT", make_rbfs, "relax_rbfs", ["DEBUG"]),
    Algorithm(
        "DCBO_MS",
        "DCBO_MS",
        make_rbfs,
        "relax_rbfs",
        ["DEBUG", "N_SAMPLES", "N_SUBQUEUES"],
    ),
    Algorithm(
        "DCBO_MS",
        "DCBO_MS_BATCHING",
        make_rbfs_batching,
        "relax_rbfs_batching",
        ["DEBUG", "N_SAMPLES", "N_SUBQUEUES", "BATCH_SIZE"],
    ),
    Algorithm(
        "DCBO_FAA",
        "DCBO_FAA",
        make_rbfs,
        "relax_rbfs",
        ["DEBUG", "N_SAMPLES", "N_SUBQUEUES"],
    ),
    Algorithm(
        "DCBO_FAA",
        "DCBO_FAA_BATCHING",
        make_rbfs_batching,
        "relax_rbfs_batching",
        ["DEBUG", "N_SAMPLES", "N_SUBQUEUES", "BATCH_SIZE"],
    ),
    Algorithm(
        "DCBO_FAA",
        "DCBO_FAA_PREDEQ",
        make_rbfs_batching_predeq,
        "relax_rbfs_batching_predeq",
        ["DEBUG", "N_SAMPLES", "N_SUBQUEUES", "BATCH_SIZE"],
    ),
    Algorithm(
        "DCBO_FAA",
        "DCBO_FAA_DEPTH_THRESH",
        make_rbfs_batching_predeq_depth_thresh,
        "relax_rbfs_batching_predeq_depth_thresh",
        ["DEBUG", "N_SAMPLES", "N_SUBQUEUES", "BATCH_SIZE"],
    ),
    Algorithm(
        "DCBO_FAA_INT",
        "DCBO_FAA_INT",
        make_rbfs,
        "relax_rbfs",
        ["DEBUG", "N_SAMPLES", "N_SUBQUEUES"],
    ),
    Algorithm(
        "FAA_BATCHING",
        "FAA_BATCHING",
        make_rbfs_batching,
        "relax_rbfs_batching",
        ["DEBUG", "BATCH_SIZE"],
    ),
]


def parse_args():
    parser = argparse.ArgumentParser(description="Benchmarking utility for BFS")
    parser.add_argument(
        "-q",
        "--queues",
        nargs="+",
        required=True,
        type=str,
        help="List of queues to run algorithms with",
        choices=[alg.display_name for alg in ALGORITHMS],
    )
    parser.add_argument(
        "-args",
        "--bfsargs",
        type=str,
        required=True,
        help="Arguments sent to the BFS binary (e.g. -g 20 -n 10 -v)",
    )
    parser.add_argument(
        "-t",
        "--threads",
        nargs="+",
        required=True,
        type=int,
        help="List of thread counts to run algorithms with",
    )
    parser.add_argument(
        "-sa",
        "--n_samples",
        nargs="+",
        type=int,
        help="List of number of samples for d-CBO queues",
        default=[DEFAULT_N_SAMPLES],
    )
    parser.add_argument(
        "-sq",
        "--n_subqueues",
        nargs="+",
        type=int,
        help="List of number of subqueues for d-CBO queues",
        default=[DEFAULT_N_SUBQUEUES],
    )
    parser.add_argument(
        "-bs",
        "--batch_sizes",
        nargs="+",
        type=int,
        help="List of batch sizes to run algorithms with",
        default=[DEFAULT_BATCH_SIZE],
    )
    parser.add_argument(
        "-d",
        "--debug",
        choices=["yes", "no", "both"],
        type=str,
        help="Debug mode",
        default=["no"],
    )
    parser.add_argument("-o", "--output", type=str, help="Output dir", required=True)
    parsed_args = parser.parse_args()

    if "both" in parsed_args.debug:
        parsed_args.debug = ["FALSE", "TRUE"]
    elif "yes" in parsed_args.debug:
        parsed_args.debug = ["TRUE"]
    else:
        parsed_args.debug = ["FALSE"]

    return Args(
        bfsargs=parsed_args.bfsargs,
        threads=parsed_args.threads,
        output_dir=parsed_args.output,
        n_samples=parsed_args.n_samples,
        n_subqueues=parsed_args.n_subqueues,
        batch_sizes=parsed_args.batch_sizes,
        debug=parsed_args.debug,
        queues=parsed_args.queues,
    )


def check_return_code(process: subprocess.CompletedProcess, command: str):
    if process.returncode != 0:
        print(
            f"Command failed with return code {process.returncode}:\n{command}",
        )
        print("Exiting...")
        exit(1)


def print_aligned(left, rest):
    left = f"{left}:"
    print(f"{left:<20} {rest}")


def run_algorithms(algorithms: list[Algorithm], args: Args):
    already_run = set()

    os.mkdir(args.output_dir)
    for algorithm, n_samples, n_subqueue, batch_size, debug in product(
        algorithms,
        args.n_samples,
        args.n_subqueues,
        args.batch_sizes,
        args.debug,
    ):
        make_command = algorithm.make_function(
            algorithm.name,
            N_SAMPLES=str(n_samples),
            N_SUBQUEUES=str(n_subqueue),
            BATCH_SIZE=str(batch_size),
            DEBUG=str(debug),
        )
        output_name = algorithm.display_name
        if "N_SAMPLES" in algorithm.compilation_flags:
            output_name += f"_SA{n_samples}"
        if "N_SUBQUEUES" in algorithm.compilation_flags:
            output_name += f"_SQ{n_subqueue}"
        if "BATCH_SIZE" in algorithm.compilation_flags:
            output_name += f"_BS{batch_size}"
        if "DEBUG" in algorithm.compilation_flags and debug == "TRUE":
            output_name += f"_debug"

        # Skip if already run, happens when algs like DO don't care about different parameters, e.g., batch size
        if output_name in already_run:
            continue
        already_run.add(output_name)

        print_aligned("Making", output_name)
        alg_output_dir = f"{args.output_dir}/{output_name}"
        os.makedirs(alg_output_dir, exist_ok=True)
        proc = subprocess.run(make_command, shell=True, capture_output=True)
        check_return_code(proc, make_command)

        for threads in args.threads:
            run_command = f"OMP_NUM_THREADS={threads} ./bin/{algorithm.executable} {args.bfsargs} -o {output_name}_{threads}"
            print_aligned("Running", run_command)
            proc = subprocess.run(run_command, shell=True, capture_output=True)
            check_return_code(proc, run_command)

        move_command = f"mv {output_name}* {alg_output_dir}"
        proc = subprocess.run(move_command, shell=True, capture_output=True)
        check_return_code(proc, move_command)


def main():
    args = parse_args()
    start = time.time()
    selected_algorithms = [alg for alg in ALGORITHMS if alg.display_name in args.queues]

    try:
        run_algorithms(selected_algorithms, args)
    except Exception as e:
        print(e)
        print("Exiting...")
        exit(1)

    elapsed_time = time.time() - start
    formatted_time = time.strftime("%H:%M:%S", time.gmtime(elapsed_time))
    print_aligned("Total time", formatted_time)
    print("Benchmarking finished")
    print("Results are in", args.output_dir)


if __name__ == "__main__":
    main()
