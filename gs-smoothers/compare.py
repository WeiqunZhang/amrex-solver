#!/usr/bin/env python3
"""Compare two tags: are the solver outputs identical, and how do the times differ?

    ./compare.py <tagA> <tagB> [--machine M]

For each case in cases.txt, the outputs of A and B are diffed with timing and
banner lines removed (so iteration residuals and error norms are compared), and
the MLMG "Iter" time is tabulated. With several repeats the minimum is used.
"""

import argparse
import glob
import os
import re
import socket
import sys

HERE = os.path.dirname(os.path.abspath(__file__))

SKIP = re.compile(r"Timers|Run time|Total Times|AMReX \(|Initializ|initialized|"
                  r"finalized|Unused|GPU|memory|Device|Pinned|Arena|MPI", re.I)
ITER = re.compile(r"Iter = ([0-9.eE+-]+)")


def machine():
    if os.environ.get("NERSC_HOST") == "perlmutter":
        return "perlmutter"
    if os.environ.get("LMOD_SYSTEM_NAME") == "frontier" or "frontier" in socket.getfqdn():
        return "frontier"
    return os.environ.get("MACHINE", socket.gethostname().split(".")[0])


def cases():
    out = []
    with open(os.path.join(HERE, "cases.txt")) as f:
        for line in f:
            s = line.split()
            if s and not s[0].startswith("#"):
                out.append(s[0])
    return out


def filtered(path):
    with open(path, errors="replace") as f:
        return [l for l in f if not SKIP.search(l)]


def iter_time(path):
    """Sum of all MLMG Iter times in one output file, or None."""
    with open(path, errors="replace") as f:
        vals = [float(m.group(1)) for m in ITER.finditer(f.read())]
    return sum(vals) if vals else None


def best(paths):
    vals = [t for t in (iter_time(p) for p in paths) if t is not None]
    return min(vals) if vals else None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("tagA")
    ap.add_argument("tagB")
    ap.add_argument("--machine", default=machine())
    ap.add_argument("--verbose", action="store_true", help="show the diff for DIFFERENT cases")
    args = ap.parse_args()

    resA = os.path.join(HERE, "results", args.machine, args.tagA)
    resB = os.path.join(HERE, "results", args.machine, args.tagB)
    print(f"machine {args.machine}:  A = {args.tagA}   B = {args.tagB}")
    print(f"{'case':<24} {'output':<10} {'A iter[s]':>10} {'B iter[s]':>10} {'B/A':>6}")
    for c in cases():
        pa = sorted(glob.glob(os.path.join(resA, f"{c}.r*.out")))
        pb = sorted(glob.glob(os.path.join(resB, f"{c}.r*.out")))
        if not pa or not pb:
            print(f"{c:<24} {'missing':<10}")
            continue
        same = filtered(pa[0]) == filtered(pb[0])
        ta, tb = best(pa), best(pb)
        fa = f"{ta:10.4f}" if ta is not None else f"{'-':>10}"
        fb = f"{tb:10.4f}" if tb is not None else f"{'-':>10}"
        ratio = f"{tb/ta:6.2f}" if ta and tb else f"{'-':>6}"
        print(f"{c:<24} {'identical' if same else 'DIFFERENT':<10} {fa} {fb} {ratio}")
        if not same and args.verbose:
            import difflib
            sys.stdout.writelines(difflib.unified_diff(filtered(pa[0]), filtered(pb[0]),
                                                       pa[0], pb[0], n=0))


if __name__ == "__main__":
    main()
