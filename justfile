_binary := if os() == "windows" { "build\\synth_front.exe" } else { "build/synth_front" }
_sim_dir := justfile_directory() / ".." / "teensy_groovebox"

default: build run

build:
    cmake -B build -S . -G Ninja
    ninja -C build

run: build
    {{_binary}}

# Run with TCP simulator (starts teensy_groovebox, then connects)
sim:
    #!/usr/bin/env bash
    set -euo pipefail
    SIM_DIR="{{_sim_dir}}"
    echo "=== Building UI lib ==="
    cd "$SIM_DIR" && just build-lib
    echo "=== Starting simulator ==="
    PYTHONPATH="$SIM_DIR" python3 -m teensy_groovebox &
    SIM_PID=$!
    sleep 1
    echo "=== Starting synth_front ==="
    {{justfile_directory()}}/{{_binary}} --host 127.0.0.1:9877 &
    SF_PID=$!
    echo "Press Ctrl+C to stop both..."
    cleanup() {
        kill $SF_PID 2>/dev/null || true
        kill $SIM_PID 2>/dev/null || true
        wait $SF_PID 2>/dev/null || true
        wait $SIM_PID 2>/dev/null || true
        echo ""
    }
    trap cleanup EXIT INT TERM
    wait $SF_PID

clean:
    rm -rf build

clean-all: clean
    rm -rf deps/_deps deps/sdl3-src deps/sdl3-build deps/sdl3-subbuild
