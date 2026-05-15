_binary := if os() == "windows" { "build\\teensy.exe" } else { "build/teensy" }

default: build run

build:
    cmake -B build -S .
    cmake --build build

run:
    build
    {{_binary}}

clean:
    rm -rf build

clean-all: clean
    rm -rf deps/_deps deps/sdl3-src deps/sdl3-build
