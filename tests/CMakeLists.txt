project(tests C)

find_package(CUnit REQUIRED)

set(CMAKE_C_STANDARD 11)
add_compile_options(-g -Wall -Wextra)
include_directories(${libsuo_SOURCE_DIR})

link_libraries(suo cunit)

add_executable(test_golay_framing test_golay_framing.c)
#add_executable(test_hdlc_framing test_hdlc_framing.c)
add_executable(test_gmsk test_gmsk.c)
add_executable(test_zmq test_zmq.c)
add_executable(test_utils test_utils.c)
