project(tests CXX)


# Setup CPPUNIT library
find_package(CPPUNIT REQUIRED)
include_directories(${CPPUNIT_INCLUDE_DIR})
link_libraries(${CPPUNIT_LIBRARY})



link_libraries(suo)
if (Matplot++_FOUND)
	link_libraries(matplot)
endif()

add_compile_options(-g -Wall -Wextra)


# Compile Unit tests
if (0)
	add_executable(test_all all.cpp utils.cpp)
else()

	# Utlity tests
	add_executable(test_utils test_utils.cpp utils.cpp)
	add_executable(test_generator test_generator.cpp)

	# Coding tests
	#add_executable(test_convolutional coding/test_convolutional.cpp)
	add_executable(test_crc coding/test_crc.cpp)
	add_executable(test_reed_solomon coding/test_reed_solomon.cpp)

	# Framing tests
	add_executable(test_golay_framing test_golay_framing.cpp utils.cpp)
	add_executable(test_hdlc_framing test_hdlc_framing.cpp utils.cpp)

	add_executable(test_gmsk test_gmsk.cpp utils.cpp)
	

	add_executable(test_bpsk test_bpsk.cpp utils.cpp)

	#add_executable(test_zmq test_zmq.cpp utils.cpp)

	if (AMQPCPP_FOUND)
		add_executable(test_amqp frame-io/test_amqp.cpp)
	endif()

endif()


# Compile EbN0 tests
if (1)

	#add_executable(test_all ebn0/all.cpp utils.cpp)

	add_executable(ebn0_bpsk_golay ebn0/bpsk_golay.cpp utils.cpp)
	
	add_executable(ebn0_gmsk_const_golay ebn0/gmsk_cont_golay.cpp utils.cpp)
	add_executable(ebn0_gmsk_const_hdlc ebn0/gmsk_cont_hdlc.cpp utils.cpp)

	add_executable(ebn0_fsk_golay ebn0/fsk_matched_golay.cpp utils.cpp)

endif()

# Random testing
add_executable(test_suomi100 test_suomi100.cpp)
add_executable(test_rssi test_rssi.cpp utils.cpp)