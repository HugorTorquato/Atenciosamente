#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/.."

PRESET="dev"
CMD="run"

usage() {
    echo "Usage: scripts/dev.sh [--preset dev|ci] [run|test|build]"
    echo "  run   (default) configure, build, then start the server"
    echo "  test            configure, build, then run the Catch2 suite via ctest"
    echo "  build           configure and build only"
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --preset)
            PRESET="$2"
            shift 2
            ;;
        run|test|build)
            CMD="$1"
            shift
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            echo "Unknown argument: $1" >&2
            usage
            exit 1
            ;;
    esac
done

if ! command -v cmake >/dev/null 2>&1 || [[ -z "${VCPKG_ROOT:-}" ]]; then
    echo "cmake/VCPKG_ROOT not found — run this inside the backend dev container:" >&2
    echo "  docker compose exec backend bash" >&2
    exit 1
fi

echo "==> Configuring ($PRESET)"
cmake --preset="$PRESET"

echo "==> Building ($PRESET)"
cmake --build --preset="$PRESET"

case "$CMD" in
    build)
        ;;
    test)
        echo "==> Running tests ($PRESET)"
        ctest --preset="$PRESET"
        ;;
    run)
        echo "==> Starting server ($PRESET)"
        exec "./build/$PRESET/atenciosamente_server"
        ;;
esac
