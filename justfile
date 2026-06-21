default:
    @just --list

_binary := if os() == "windows" { "build/synth_front.exe" } else { "build/synth_front" }

_cmake := if os() == "windows" { '"/c/Program Files/CMake/bin/cmake.exe"' } else { "cmake" }
_cmake_flags := if os() == "windows" {
    "-G Ninja -DCMAKE_C_COMPILER=/mingw64/bin/gcc.exe -DCMAKE_CXX_COMPILER=/mingw64/bin/g++.exe -DCMAKE_MAKE_PROGRAM=/mingw64/bin/ninja.exe"
} else {
    "-G Ninja"
}

_path_prefix := if os() == "windows" { "PATH=\"/mingw64/bin:/usr/bin:$PATH\" " } else { "" }

build:
    {{_path_prefix}}{{_cmake}} -B build -S . {{_cmake_flags}} && {{_cmake}} --build build

run *args: build
    {{_path_prefix}}{{_binary}} {{args}}

list:
    @just --list

clean:
    rm -rf build

clean-all: clean
    rm -rf deps
