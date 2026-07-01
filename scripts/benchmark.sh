#!/usr/bin/env bash
# Load-test the server with wrk (https://github.com/wg/wrk).
# Install: sudo apt-get install wrk   (or build from source)
#
# Usage: ./scripts/benchmark.sh [port] [duration] [threads] [connections]
set -euo pipefail

PORT=${1:-8080}
DURATION=${2:-10s}
THREADS=${3:-4}
CONNECTIONS=${4:-100}

if ! command -v wrk &> /dev/null; then
    echo "wrk is not installed. Install it with: sudo apt-get install wrk"
    exit 1
fi

echo "Benchmarking http://localhost:${PORT}/ for ${DURATION}"
echo "  wrk threads: ${THREADS}, connections: ${CONNECTIONS}"
echo ""
wrk -t"${THREADS}" -c"${CONNECTIONS}" -d"${DURATION}" "http://localhost:${PORT}/"
