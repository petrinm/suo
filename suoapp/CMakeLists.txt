
project(suoapp C)

set(CMAKE_C_STANDARD 11)
add_compile_options(-g -Wall -Wextra)

#
add_executable(suoapp suoapp.c)
target_include_directories(suoapp ${libsuo_SOURCE_DIR})
target_link_libraries(suoapp suo)
