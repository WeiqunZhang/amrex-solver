#!/bin/bash
# Build the test executables from one AMReX source tree.
#
#   ./build.sh <amrex_source_dir> <tag> [test ...]
#
# Executables land in bin/<tag>/<test>, build logs in logs/<MACHINE>/.
# Tests: cell3d cell2d nodal3d nodal2d overset3d nodetensor3d pfs3d (default: all).
# Build <tag>=base from the merge base and <tag>=pr from the PR branch, then
# ./run.sh and ./compare.py.

set -uo pipefail

HERE=$(cd "$(dirname "$0")" && pwd)
source "$HERE/env.sh"

if [[ $# -lt 2 ]]; then
    echo "usage: $0 <amrex_source_dir> <tag> [test ...]" >&2
    exit 1
fi
AMREX=$(cd "$1" && pwd)
TAG=$2
shift 2
TESTS=("$@")
if [[ ${#TESTS[@]} -eq 0 ]]; then
    TESTS=(cell3d cell2d nodal3d nodal2d overset3d nodetensor3d pfs3d)
fi

BIN=$HERE/bin/$TAG
LOG=$HERE/logs/$MACHINE
mkdir -p "$BIN" "$LOG"

echo "AMReX source: $AMREX ($(git -C "$AMREX" rev-parse --short HEAD 2>/dev/null || echo 'not a git repo'))"
echo "Machine: $MACHINE   make flags: $MAKEFLAGS_GPU   jobs: $NJ"

status=0

# build_one <name> <dir> <dim>
build_one () {
    local name=$1 dir=$2 dim=$3
    # Separate object dirs per tag, so two source trees never share objects.
    local mk="make DIM=$dim $MAKEFLAGS_GPU AMREX_HOME=$AMREX TMP_BUILD_DIR=tmp_build_dir_$TAG"
    echo "== $name  ($dir, DIM=$dim)"
    if ! ( cd "$dir" && $mk -j"$NJ" > "$LOG/build-$TAG-$name.log" 2>&1 ); then
        echo "   FAILED, see $LOG/build-$TAG-$name.log"
        status=1
        return
    fi
    local exe
    exe=$( cd "$dir" && $mk print-executable 2>/dev/null | awk '/^executable is/{print $3}' )
    if [[ -z $exe || ! -x $dir/$exe ]]; then
        echo "   FAILED to locate executable ($exe)"
        status=1
        return
    fi
    cp "$dir/$exe" "$BIN/$name"
    echo "   -> bin/$TAG/$name  ($exe)"
}

for t in "${TESTS[@]}"; do
    case $t in
        cell3d)       build_one cell3d       "$AMREX/Tests/LinearSolvers/ABecLaplacian_C" 3 ;;
        cell2d)       build_one cell2d       "$AMREX/Tests/LinearSolvers/ABecLaplacian_C" 2 ;;
        nodal3d)      build_one nodal3d      "$HERE/nodal_variants" 3 ;;
        nodal2d)      build_one nodal2d      "$HERE/nodal_variants" 2 ;;
        overset3d)    build_one overset3d    "$AMREX/Tests/LinearSolvers/CellOverset" 3 ;;
        nodetensor3d) build_one nodetensor3d "$AMREX/Tests/LinearSolvers/NodeTensorLap" 3 ;;
        pfs3d)        build_one pfs3d        "$AMREX/Tests/Base/ParallelForStrided" 3 ;;
        *) echo "unknown test: $t"; status=1 ;;
    esac
done

exit $status
