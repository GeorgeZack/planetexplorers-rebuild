#!/usr/bin/env python3
"""Parallel front end for the Makefile's test targets.

The Makefile stays the single description of what a target builds and runs.
This tool asks `make -n --trace` for that plan, builds every out-of-date
binary with `make -j`, then runs the remaining commands concurrently. Unlike
`make test` it continues past failures, keeps one log per command and totals
unittest skips, so an asset-dependent skip or a late failure is not hidden.

    python3 tools/run_tests.py                 # everything `make test` covers
    python3 tools/run_tests.py sanitize        # ASan/UBSan, one process per suite
    python3 tools/run_tests.py -k hunt_loot    # only matching commands/suites
    python3 tools/run_tests.py --list

A recipe whose lines are all plain test invocations is split into independent
jobs, and build/test_core and build/test_sanitize are split into one process
per registered suite, each with its own exit-time leak scan.
Any other recipe keeps its lines together and in order.
"""
import argparse
import concurrent.futures
import json
import os
from pathlib import Path
import re
import shlex
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parent.parent
TRACE = re.compile(r"^\S+:\d+: (?:update target|target) '([^']+)'")
BUILD_OUTPUT = re.compile(r" -o (build/\S+)")
ENV = r"(?:\w+=(?:'[^']*'|\S+) )*"
SIMPLE = re.compile(rf"^{ENV}(?:\S*python[\d.]* (?:tests|tools)/\S+\.py(?: --check)?|\./build/\w+)$")
# Binaries built from tests/test_main.c: one process per registered suite.
REGISTRY = re.compile(rf"^{ENV}(\./build/(test_core|test_sanitize))$")
RAN = re.compile(r"^Ran (\d+) tests? in", re.M)
SKIPPED = re.compile(r"skipped=(\d+)")


def make(*args, **kwargs):
    return subprocess.run(["make", "--no-print-directory", *args], cwd=ROOT, text=True, **kwargs)


def plan(targets):
    """Return (build outputs, [(target, [command lines])]) from a dry run."""
    out = make("-n", "--trace", "-k", *targets, stdout=subprocess.PIPE, stderr=subprocess.STDOUT).stdout
    builds, recipes, pending = [], [], ""
    for raw in out.splitlines():
        match = TRACE.match(raw)
        if match:
            recipes.append((match.group(1), []))
            continue
        if raw.startswith("make: ") or raw.startswith("make["):
            if "Error" in raw or "No rule" in raw or "Stop" in raw:
                sys.exit(f"make could not plan {' '.join(targets)}:\n{out}")
            continue
        line = pending + raw
        if line.endswith("\\"):
            pending = line[:-1]
            continue
        pending = ""
        if not line.strip() or not recipes:
            continue
        recipes[-1][1].append(line)
    runs = []
    for target, lines in recipes:
        if target.startswith("build/"):
            # A file target's recipe is a build step; make runs it for us.
            if not target.startswith("build/obj/"):
                builds.append(target)
            continue
        if lines:
            runs.append((target, lines))
    return builds, runs


def jobs_from(runs):
    jobs = []
    for target, lines in runs:
        if all(SIMPLE.match(line) for line in lines):
            for line in lines:
                registry = REGISTRY.match(line)
                if registry:
                    names = subprocess.run([registry.group(1), "--list"], cwd=ROOT, text=True, check=True,
                                           stdout=subprocess.PIPE).stdout.split()
                    prefix = "core" if registry.group(2) == "test_core" else "sanitize"
                    jobs += [(f"{prefix}:{name}", [f"{line} {name}"]) for name in names]
                else:
                    jobs.append((Path(line.split()[-1]).stem, [line]))
        else:
            jobs.append((target, lines))
    return jobs


def run_job(job, log_dir, env):
    name, lines = job
    start = time.monotonic()
    output, code = "", 0
    for line in lines:
        result = subprocess.run(line, shell=True, cwd=ROOT, env=env, text=True, errors="replace",
                                stdin=subprocess.DEVNULL, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        output += f"$ {line}\n{result.stdout}"
        code = result.returncode
        if code:
            break
    log = log_dir / (re.sub(r"[^\w.-]+", "_", name) + ".log")
    log.write_text(output)
    return name, code, time.monotonic() - start, log, output


def compile_commands():
    """Write compile_commands.json for clangd/clang-tidy from the object rules."""
    out = make("-n", "-B", "all", "build/test_core", stdout=subprocess.PIPE, check=True).stdout
    entries = []
    for line in out.splitlines():
        parts = line.split()
        if "-c" in parts and "-o" in parts:
            entries.append({"directory": str(ROOT), "command": line, "file": parts[parts.index("-c") + 1]})
    (ROOT / "compile_commands.json").write_text(json.dumps(entries, indent=1) + "\n")
    print(f"compile_commands.json: {len(entries)} translation units")


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("targets", nargs="*", default=["test"], help="make targets (default: test)")
    parser.add_argument("-j", "--jobs", type=int, default=os.cpu_count() or 2)
    parser.add_argument("-k", "--filter", action="append", default=[], help="run only jobs containing this text")
    parser.add_argument("--list", action="store_true", help="print the jobs without running them")
    parser.add_argument("--no-build", action="store_true", help="fail instead of building stale binaries")
    parser.add_argument("--slowest", type=int, default=8, help="how many slow jobs to report")
    parser.add_argument("--compile-commands", action="store_true", help="write compile_commands.json and exit")
    args = parser.parse_args()
    if args.compile_commands:
        return compile_commands()

    env = dict(os.environ)
    tmp = ROOT / "build" / "compiler-tmp"
    tmp.mkdir(parents=True, exist_ok=True)
    env.setdefault("TMPDIR", str(tmp))
    os.environ.setdefault("TMPDIR", str(tmp))

    started = time.monotonic()
    builds, runs = plan(args.targets)
    if builds:
        if args.no_build:
            sys.exit(f"{len(builds)} stale binaries; rerun without --no-build")
        print(f"building {len(builds)} binaries with -j{args.jobs} ...", flush=True)
        built = make(f"-j{args.jobs}", "-k", *builds, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        if built.returncode:
            print(built.stdout)
            sys.exit("BUILD FAILED")
        builds, runs = plan(args.targets)
        if builds:
            sys.exit(f"still stale after building: {' '.join(builds)}")
    build_seconds = time.monotonic() - started

    jobs = jobs_from(runs)
    if args.filter:
        jobs = [job for job in jobs if any(f in job[0] or any(f in line for line in job[1]) for f in args.filter)]
    if args.list:
        for name, lines in jobs:
            print(f"{name}: {' && '.join(lines)}")
        return 0
    if not jobs:
        sys.exit("no jobs selected")

    log_dir = ROOT / "build" / "test-logs"
    log_dir.mkdir(parents=True, exist_ok=True)
    results, tests, skipped = [], 0, 0
    with concurrent.futures.ThreadPoolExecutor(max(1, args.jobs)) as pool:
        for name, code, seconds, log, output in pool.map(lambda job: run_job(job, log_dir, env), jobs):
            results.append((name, code, seconds, log))
            tests += sum(int(n) for n in RAN.findall(output))
            job_skips = sum(int(n) for n in SKIPPED.findall(output))
            skipped += job_skips
            if code:
                print(f"FAIL {name} (exit {code}, {seconds:.1f}s) {log.relative_to(ROOT)}")
                print("    " + "\n    ".join(output.rstrip().splitlines()[-25:]), flush=True)
            elif job_skips:
                print(f"skip {name}: {job_skips} skipped", flush=True)

    failed = [r for r in results if r[1]]
    print(f"\nslowest of {len(results)} jobs:")
    for name, _, seconds, _ in sorted(results, key=lambda r: -r[2])[:args.slowest]:
        print(f"  {seconds:6.1f}s  {name}")
    print(f"\n{len(results) - len(failed)} passed, {len(failed)} failed; {tests} Python tests, {skipped} skipped "
          f"(skips are not passes); build {build_seconds:.0f}s, total {time.monotonic() - started:.0f}s; "
          f"logs in {log_dir.relative_to(ROOT)}/")
    for name, code, _, log in failed:
        print(f"  FAILED {name}: {shlex.quote(str(log.relative_to(ROOT)))}")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
