cmake_minimum_required(VERSION 3.10)

project(CipherNode)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(include)

set(ENGINE_SOURCES
    src/engine.cc
    src/binary_storage.cc
    src/sstable.cc
    src/compaction.cc
)

add_executable(engine_test
    src/main.cc
    ${ENGINE_SOURCES}
)

add_executable(bench
    tests/bench.cc
    ${ENGINE_SOURCES}
)