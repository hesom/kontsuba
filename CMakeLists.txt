cmake_minimum_required(VERSION 3.18)

project(kontsuba LANGUAGES C CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON CACHE BOOL "Export compile commands")
set(CMAKE_POSITION_INDEPENDENT_CODE ON CACHE BOOL "")

# build dependencies
add_subdirectory(dependencies)

# build libraries and executable
add_subdirectory(kontsuba)