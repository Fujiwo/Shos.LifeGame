#!/usr/bin/env bash
# Builds and runs the regression tests for all 16 combinations of USEBITS / FAST / MT / AREA.
#
# Usage: Shos.LifeGame.Test/run_regression_tests.sh [--asan]
#   --asan  Build with AddressSanitizer and UndefinedBehaviorSanitizer (slow: about a minute per combination).
#
# Requires g++ (C++20). Works on Linux and WSL.

set -u

script_folder="$(cd "$(dirname "$0")" && pwd)"
source_file="$script_folder/Shos.LifeGame.RegressionTest.cpp"
cell_data_folder="$script_folder/../Shos.LifeGame/CellData"
build_folder="${TMPDIR:-/tmp}/ShosLifeGameRegressionTest"
compiler="${CXX:-g++}"

flags=(-std=c++20 -O1 -g -pthread -D_DEBUG -DSHOS_LIFEGAME_CUSTOM_CONFIG -I"$script_folder/LinuxCompat")
if [[ "${1:-}" == "--asan" ]]; then
    flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer)
fi

mkdir -p "$build_folder"

options=(USEBITS FAST MT AREA)
names=()
pids=()

# Build all combinations in parallel.
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
    if ! "$build_folder/$name" "$cell_data_folder" > "$build_folder/$name.log" 2>&1; then
        failed+=("$name")
        cat "$build_folder/$name.log"
    else
        printf '%-22s %s\n' "$name" "$(tail -n 1 "$build_folder/$name.log")"
    fi
done

echo
if (( ${#failed[@]} == 0 )); then
    echo "All 16 combinations passed. (Logs: $build_folder)"
else
    echo "Failed: ${failed[*]}"
    exit 1
fi
