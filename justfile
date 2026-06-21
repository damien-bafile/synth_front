set shell := ["C:\\msys64\\usr\\bin\\bash.exe", "-c"]

default:
    @just --list

_binary := if os() == "cygwin" { "build/synth_front.exe" } else { "build/synth_front" }

_cmake := if os() == "cygwin" { '"/c/Program Files/CMake/bin/cmake.exe"' } else { "cmake" }
_cmake_flags := if os() == "cygwin" {
    "-G Ninja -DCMAKE_C_COMPILER=/mingw64/bin/gcc.exe -DCMAKE_CXX_COMPILER=/mingw64/bin/g++.exe -DCMAKE_MAKE_PROGRAM=/mingw64/bin/ninja.exe"
} else {
    "-G Ninja"
}

build:
    PATH="/mingw64/bin:/usr/bin:$PATH" {{_cmake}} -B build -S . {{_cmake_flags}} && {{_cmake}} --build build

run *args: build
    PATH="/mingw64/bin:$PATH" {{_binary}} {{args}}

list:
    @just --list

clean:
    PATH="/usr/bin:$PATH" rm -rf build

clean-all: clean
    PATH="/usr/bin:$PATH" rm -rf deps
