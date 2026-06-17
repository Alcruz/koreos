#!/usr/bin/env bash
set -euo pipefail

# Helper to enter the toolchain container with the repo mounted.
# Usage: scripts/toolchain-shell.sh [command]

COMPOSE_FILE=toolchain/docker-compose.yml

if command -v docker >/dev/null 2>&1; then
  docker compose -f "$COMPOSE_FILE" run --rm toolchain ${@:-bash}
else
  echo "docker not found. Install Docker Desktop or Docker Engine to use the toolchain container."
  exit 1
fi
