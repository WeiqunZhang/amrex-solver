#!/bin/bash
# Run the cases in cases.txt with the executables of one tag.
#
#   ./run.sh <tag> [case ...]          (default: all cases)
#   REPEAT=3 ./run.sh pr               (run each case 3 times)
#
# Output goes to results/<MACHINE>/<tag>/<case>.r<N>.out. Run inside a job
# allocation; LAUNCH from env.sh prefixes each command (srun on the clusters).

set -uo pipefail

HERE=$(cd "$(dirname "$0")" && pwd)
source "$HERE/env.sh"

if [[ $# -lt 1 ]]; then
    echo "usage: $0 <tag> [case ...]" >&2
    exit 1
fi
TAG=$1
shift
ONLY=("$@")
REPEAT=${REPEAT:-1}

RES=$HERE/results/$MACHINE/$TAG
mkdir -p "$RES"
echo "Machine: $MACHINE   tag: $TAG   launcher: '${LAUNCH}'   repeat: $REPEAT"

while read -r name exe inputs _; do
    [[ -z $name || $name == \#* ]] && continue
    if [[ ${#ONLY[@]} -gt 0 ]] && ! printf '%s\n' "${ONLY[@]}" | grep -qx "$name"; then
        continue
    fi
    bin=$HERE/bin/$TAG/$exe
    if [[ ! -x $bin ]]; then
        echo "$name: missing $bin (run build.sh)"
        continue
    fi
    inp=""
    [[ $inputs != - ]] && inp=$HERE/cases/$inputs
    for ((r = 1; r <= REPEAT; ++r)); do
        out=$RES/$name.r$r.out
        $LAUNCH "$bin" $inp > "$out" 2>&1
        rc=$?
        summary=$(grep -o 'Iter = [0-9.e+-]*' "$out" | tr '\n' ' ')
        summary+=$(grep -o '[0-9]* cases, [0-9]* failures' "$out")
        printf '%-24s r%d rc=%d  %s\n' "$name" "$r" "$rc" "$summary"
    done
done < "$HERE/cases.txt"
