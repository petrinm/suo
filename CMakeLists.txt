project(suo)
cmake_minimum_required(VERSION 3.1)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

enable_testing()

set(CMAKE_BUILD_TYPE Debug) # Force debug mode

add_compile_options(-g -Wall -Wextra)
add_compile_options(-Werror=incompatible-pointer-types -Wno-unused)
#add_compile_options(-O2)

add_subdirectory(libsuo)
#add_subdirectory(suoapp)
add_subdirectory(skymodem)
#add_subdirectory(tests)
