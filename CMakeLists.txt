project(suo)
cmake_minimum_required(VERSION 3.16)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(CMakeFindDependencyMacro)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

enable_testing()

set(CMAKE_BUILD_TYPE Debug) # Force debug mode


add_compile_options(-g -Wall -Wextra)
add_compile_options(-Wno-unused)

add_subdirectory(libsuo)
add_subdirectory(skymodem)
add_subdirectory(tests)
