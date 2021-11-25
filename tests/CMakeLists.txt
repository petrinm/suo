project(tests C)

#add_compile_options(-L/home/aalto/suo.git/build/libsuo)

find_package(CUnit REQUIRED)

set(CMAKE_C_STANDARD 11)
add_compile_options(-g -Wall -Wextra)

link_libraries(suo cunit)
link_libraries(liquid zmq SoapySDR) # <<-- Why Cmake won't resolve this!

add_executable(test_golay_framing test_golay_framing.c)
#add_executable(test_hdlc_framing test_hdlc_framing.c)
add_executable(test_gmsk test_gmsk.c)
add_executable(test_zmq test_zmq.c)
add_executable(test_utils test_utils.c)
add_executable(test_txrx test_txrx.c)
add_executable(test_ebno test_ebno.c)
