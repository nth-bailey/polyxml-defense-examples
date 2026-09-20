cmake_minimum_required(VERSION 3.20)
project(lattice_uci_bridge_cpp CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(lattice_uci_bridge main.cpp)

target_include_directories(lattice_uci_bridge PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../generated/cpp
)
