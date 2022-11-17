project(skymodem CXX)

add_compile_options(-g -Wall -Wextra)

add_executable(skymodem skymodem.cpp)
target_link_libraries(skymodem PRIVATE suo)
