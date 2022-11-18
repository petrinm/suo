project(tests CXX)


# Setup CPPUNIT library
find_package(CPPUNIT REQUIRED)
include_directories(${CPPUNIT_INCLUDE_DIR})
link_libraries(${CPPUNIT_LIBRARY})


link_libraries(suo matplot)
add_compile_options(-g -Wall -Wextra)


add_executable(test_golay_framing test_golay_framing.cpp utils.cpp)
add_executable(test_hdlc_framing test_hdlc_framing.cpp utils.cpp)
add_executable(test_gmsk test_gmsk.cpp utils.cpp)
add_executable(test_fsk test_fsk.cpp utils.cpp)
add_executable(test_bpsk test_bpsk.cpp utils.cpp)
#add_executable(test_zmq test_zmq.cpp utils.cpp)
#add_executable(test_utils test_utils.cpp utils.cpp)
add_executable(test_ebno test_ebno.cpp utils.cpp)