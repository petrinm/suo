#
# Receipt for baking the Suo-library
#

project(suo C)
#set(CMAKE_C_STANDARD 11)

add_compile_options(-g -Wall -Wextra)


# Setup building of the library
add_library(suo SHARED
    suo.c
    metadata.c
#    mux.c
#    modem/burst_dpsk_receiver.c
    modem/demod_fsk_corrbank.c
    modem/demod_fsk_mfilt.c
#    modem/demod_fsk_quad.c
    modem/demod_gmsk.c
    modem/mod_fsk.c
    modem/mod_gmsk.c
#    modem/mod_psk.c
    framing/golay24.c
    framing/golay_deframer.c
    framing/golay_framer.c
    framing/hdlc_deframer.c
    framing/hdlc_framer.c
    framing/syncword_deframer.c
    framing/syncword_framer.c
    framing/tetra_deframer.c
    framing/utils.c
    frame-io/zmq_input.c
    frame-io/zmq_output.c
#    frame-io/file_dump.c
    signal-io/file_io.c
    signal-io/soapysdr_io.c
)



include(CMakeFindDependencyMacro)

target_include_directories(suo PUBLIC ${PROJECT_SOURCE_DIR})
target_link_libraries(suo PUBLIC m)

# Setup SoapySDR
find_dependency(SoapySDR REQUIRED)
target_include_directories(suo PRIVATE ${SoapySDR_INCLUDE_DIRS})
target_link_libraries(suo PUBLIC ${SoapySDR_LIBRARIES})

# Setup Liquid-DSP
find_dependency(LiquidDSP REQUIRED)
target_include_directories( suo PRIVATE ${LIQUIDDSP_INCLUDE_DIRS})
target_link_libraries(suo PUBLIC ${LIQUID_LIBRARIES})

# Setup ZMQ
find_dependency(ZeroMQ REQUIRED)
target_include_directories( suo PUBLIC ${ZMQ_INCLUDE_DIRS})
target_link_libraries(suo PUBLIC ${ZMQ_LIBRARIES})


target_compile_features(suo PUBLIC cxx_std_11)


# Find ALSA
find_package(ALSA)
if (ALSA_FOUNDD)
    target_sources(suo PRIVATE signal-io/alsa_io.c)
    add_definitions(-DENABLE_ALSA)
    target_link_libraries(suo PUBLIC ${ALSA_LIBRARIES})
endif()


install( TARGETS suo LIBRARY DESTINATION lib )
#install( FILES suo.h DESTINATION include )
install( DIRECTORY . DESTINATION include/suo FILES_MATCHING PATTERN "*.h" )
