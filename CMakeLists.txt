project(suo)
cmake_minimum_required(VERSION 3.1)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

enable_testing()

set(CMAKE_BUILD_TYPE Debug) # Force debug mode

add_subdirectory(libsuo)
add_subdirectory(suoapp)
