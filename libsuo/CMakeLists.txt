#
# Receipt for baking the Suo-library
#

project(suo C)
set(CMAKE_C_STANDARD 11)


# Setup SoapySDR
find_package(SoapySDR REQUIRED)
include_directories(${SOAYPSDR_INCLUDE_DIRS})


# Setup Liquid-DSP
find_package(LiquidDSP REQUIRED)
include_directories(${LIQUIDDSP_INCLUDES})

# Setup ZMQ
find_package(ZeroMQ REQUIRED)

add_compile_options(-g -Wall -Wextra)

# Setup building of the library
add_library(suo SHARED
    suo.c
    metadata.c
#    modem/burst_dpsk_receiver.c
    modem/demod_fsk_corrbank.c
    modem/demod_fsk_mfilt.c
    modem/demod_fsk_quad.c
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
    signal-io/file_io.c
    signal-io/soapysdr_io.c
)


target_include_directories(suo PUBLIC ${PROJECT_SOURCE_DIR})

target_link_libraries(suo ${SoapySDR_LIBRARIES})
target_link_libraries(suo ${ZMQ_LIBRARIES})
target_link_libraries(suo ${LIQUID_LIBRARIES})


# Find ALSA
find_package(ALSA)
if (ALSA_FOUND)
    link_libraries(alsa)
    target_sources(suo PRIVATE signal-io/alsa_io.c)
    #target_link_libraries(suo PUBLIC ${ALSA_LIBRARY})
endif()
