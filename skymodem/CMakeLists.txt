project(skymodem C)

set(CMAKE_C_STANDARD 11)
add_compile_options(-g -Wall -Wextra)

add_executable(skymodem skymodem.c)
target_link_libraries(skymodem PRIVATE suo)
target_link_libraries(skymodem PRIVATE liquid zmq SoapySDR) # <<-- Why Cmake won't resolve this!
