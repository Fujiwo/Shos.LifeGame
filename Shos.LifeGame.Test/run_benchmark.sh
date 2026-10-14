#!/usr/bin/env bash
# Builds and runs the benchmark of Game::Next for the combinations of USEBITS / FAST / MT / AREA.
#
# Usage: Shos.LifeGame.Test/run_benchmark.sh [--scale <number>] [combination...]
#   --scale <number>  Multiplies the number of the generations of each case (default: 1.0).
#   combination       Names such as USEBITS_FAST_MT_AREA or NONE (default: all the 16 combinations).
#
# The benchmarks run one by one, so that they do not disturb each other.
# Requires g++ (C++20). Works on Linux and WSL.

set -u

script_folder="$(cd "$(dirname "$0")" && pwd)"
source_file="$script_folder/Shos.LifeGame.Benchmark.cpp"
cell_data_folder="$script_folder/../Shos.LifeGame/CellData"
build_folder="${TMPDIR:-/tmp}/ShosLifeGameBenchmark"
compiler="${CXX:-g++}"

flags=(-std=c++20 -O2 -DNDEBUG -pthread -DSHOS_LIFEGAME_CUSTOM_CONFIG -I"$script_folder/LinuxCompat")

scale=1.0
selected=()
while (( $# > 0 )); do
    case "$1" in
        --scale) scale="$2"; shift 2 ;;
        *)       selected+=("$1"); shift ;;
    esac
done

mkdir -p "$build_folder"

options=(USEBITS FAST MT AREA)
names=()
pids=()

# Build the combinations in parallel.
for mask in $(seq 0 15); do
    defines=()
    name=""
    for bit in 0 1 2 3; do
        if (( mask & (1 << bit) )); then
            defines+=("-D${options[$bit]}")
            name+="${options[$bit]}_"
        fi
    done
    name="${name%_}"
    name="${name:-NONE}"
    if (( ${#selected[@]} > 0 )) && [[ ! " ${selected[*]} " == *" $name "* ]]; then
        continue
    fi
    names+=("$name")
    "$compiler" "${flags[@]}" "${defines[@]}" "$source_file" -o "$build_folder/$name" > "$build_folder/$name.build.log" 2>&1 &
    pids+=($!)
done

failed=()
for index in "${!names[@]}"; do
    name="${names[$index]}"
    if ! wait "${pids[$index]}"; then
        echo "BUILD FAILED: $name"
        cat "$build_folder/$name.build.log"
        failed+=("$name (build)")
    fi
done

for name in "${names[@]}"; do
    [[ -x "$build_folder/$name" ]] || continue
    echo "== $name"
    "$build_folder/$name" "$cell_data_folder" "$scale" || failed+=("$name")
done

if (( ${#failed[@]} > 0 )); then
    echo "Failed: ${failed[*]}"
    exit 1
fi
