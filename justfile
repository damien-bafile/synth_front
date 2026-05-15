set shell := ["bash", "-c"]
default:
    @just --list

_binary := if os() == "windows" { "build\\synth_front.exe" } else { "build/synth_front" }

build:
    cmake -B build -S . -G Ninja
    ninja -C build

run: build
    {{_binary}}

clean:
    rm -rf build

clean-all: clean
    rm -rf deps/_deps deps/sdl3-src deps/sdl3-build deps/sdl3-subbuild
