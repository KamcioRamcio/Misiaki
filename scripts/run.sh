#!/usr/bin/env bash
# Wrapper uruchomieniowy: ./scripts/run.sh <N> <K> <M> [seed]
# Przy obecności pliku ./hosts uruchamia z hostfile.

set -euo pipefail

@@ -9,24 +9,61 @@ if [[ $# -lt 3 ]]; then
    exit 1
fi

N=$1
K=$2
M=$3
SEED=${4:-}

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BIN="$ROOT/okrety"

if [[ ! -x $BIN ]]; then
    echo "Brak $BIN — uruchom 'make' najpierw." >&2
    exit 1
fi

ARGS=("$K" "$M")
[[ -n $SEED ]] && ARGS+=("$SEED")

if [[ -f "$ROOT/hosts" ]]; then
    mpirun --hostfile "$ROOT/hosts" -np "$N" "$BIN" "${ARGS[@]}"
else
    mpirun -np "$N" "$BIN" "${ARGS[@]}"
fi