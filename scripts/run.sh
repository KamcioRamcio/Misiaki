#!/usr/bin/env bash
# Wrapper uruchomieniowy: ./scripts/run.sh <N> <K> <M> [seed]
# Obsługuje różne ścieżki binaru na lokalnym (Linux) i zdalnym (macOS).

set -euo pipefail

if [[ $# -lt 3 ]]; then
    echo "Użycie: $0 <N> <K> <M> [seed]" >&2
    exit 1
fi

N=$1; K=$2; M=$3; SEED=${4:-}

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BIN="$ROOT/okrety"

# Ścieżka binaru na zdalnej maszynie (macOS).
REMOTE_BIN="/Users/kamilek/Desktop/misiaki/Misiaki/okrety"

ARGS="$K $M${SEED:+ $SEED}"

if [[ ! -f "$ROOT/hosts" ]]; then
    make -C "$ROOT"
    mpirun -np "$N" "$BIN" $ARGS
    exit 0
fi

# Kompiluj lokalnie.
echo ">> make (lokalnie)"
make -C "$ROOT"

# Zbierz lokalne IP.
local_ips=$(ip addr show 2>/dev/null | awk '/inet / {split($2,a,"/"); print a[1]}')

# Generuj appfile - każdy host dostaje właściwą ścieżkę binaru i liczbę slotów.
APPFILE=$(mktemp /tmp/mpi_appfile.XXXXXX)
trap 'rm -f "$APPFILE"' EXIT

total=0
while IFS= read -r line || [[ -n $line ]]; do
    [[ -z $line || $line == \#* ]] && continue
    host=$(echo "$line" | awk '{print $1}')
    slots=$(echo "$line" | awk '{print $2}' | grep -oP '\d+' || echo 1)

    # Nie przekraczaj N procesów łącznie.
    remaining=$(( N - total ))
    [[ $remaining -le 0 ]] && break
    np=$(( slots < remaining ? slots : remaining ))
    total=$(( total + np ))

    resolved=$(getent hosts "$host" 2>/dev/null | awk '{print $1}' | head -1)
    [[ -z $resolved ]] && resolved=$host

    is_local=0
    for lip in $local_ips; do
        [[ $resolved == "$lip" || $host == "$lip" ]] && { is_local=1; break; }
    done

    if [[ $is_local -eq 1 ]]; then
        echo "-H $host -np $np $BIN $ARGS" >> "$APPFILE"
    else
        echo "-H $host -np $np $REMOTE_BIN $ARGS" >> "$APPFILE"
    fi
done < "$ROOT/hosts"

echo ">> appfile:"
cat "$APPFILE"
echo ">> mpirun"
mpirun --app "$APPFILE"
