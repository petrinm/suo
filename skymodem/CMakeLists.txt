project(skymodem C)

set(CMAKE_C_STANDARD 11)
add_compile_options(-g -Wall -Wextra)

#
add_executable(skymodem skymodem.c)

include_directories(${suo_SOURCE_DIR})
#target_include_directories(skymodem ${suo_SOURCE_DIR})
target_link_libraries(skymodem suo)
