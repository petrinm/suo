#
# Receipt for baking the Suo-library
#

project(suo LANGUAGES CXX)

add_compile_options(-g -Wall -Wextra)


# Setup building of the library
add_library(suo SHARED
    suo.cpp
    frame.cpp
    generators.cpp
#    modem/demod_fsk_corrbank.cpp
    modem/demod_fsk_mfilt.cpp
#    modem/demod_fsk_quad.cpp
    modem/demod_gmsk_cont.cpp
    modem/demod_gmsk.cpp
    modem/demod_psk.cpp
    modem/mod_fsk.cpp
    modem/mod_gmsk.cpp
    modem/mod_psk.cpp
    coding/convolutional_encoder.cpp
#    coding/differential.cpp
    coding/golay24.cpp
    coding/randomizer.cpp
    coding/reed_solomon.cpp
#    coding/viterbi_decoder.cpp
    coding/crc.cpp
    framing/golay_deframer.cpp
    framing/golay_framer.cpp
    framing/hdlc_deframer.cpp
    framing/hdlc_framer.cpp
    framing/syncword_deframer.cpp
    framing/syncword_framer.cpp
#    framing/tetra_deframer.cpp
    framing/utils.cpp
    frame-io/zmq_interface.cpp
    frame-io/file_dump.cpp
    signal-io/file_io.cpp
    signal-io/soapysdr_io.cpp
    misc/rigctl.cpp
    misc/random_symbols.cpp
)


target_include_directories(suo PUBLIC ${PROJECT_SOURCE_DIR})

# Setup Nlohmann's JSON library
target_include_directories(suo PRIVATE ../nlohmann)

# Setup SoapySDR
find_package(SoapySDR REQUIRED)
target_include_directories(suo PUBLIC ${SoapySDR_INCLUDE_DIRS})
target_link_libraries(suo PUBLIC ${SoapySDR_LIBRARIES})

# Setup LiquidDSP
find_package(LiquidDSP REQUIRED)
target_include_directories(suo PRIVATE ${LiquidDSP_INCLUDE_DIRS})
target_link_libraries(suo PUBLIC ${LiquidDSP_LIBRARIES})

# Setup CPPZMQ
find_package(CPPZMQ REQUIRED)
target_include_directories(suo PUBLIC ${CPPZMQ_INCLUDE_DIRS})
target_link_libraries(suo PUBLIC ${CPPZMQ_LIBRARIES})

# Setup AMQPCPP
find_package(AMQPCPP)
if (AMQPCPP_FOUND)
    target_include_directories(suo PUBLIC ${AMQPCPP_INCLUDE_DIRS})
    target_link_libraries(suo PUBLIC ${AMQPCPP_LIBRARIES})
    target_compile_definitions(suo PUBLIC SUO_SUPPORT_AMQP)
    target_sources(suo PRIVATE 
        frame-io/amqp_interface.cpp
        misc/porthouse_tracker.cpp
    )
endif()

# Find Matplot++
find_package(Matplot++)
if (Matplot++_FOUND)
    target_include_directories(suo PRIVATE ${Matplot++_INCLUDE_DIRS})
    target_link_libraries(suo PUBLIC Matplot++::matplot)
    target_compile_definitions(suo PUBLIC SUO_SUPPORT_PLOTTING)
    target_sources(suo PRIVATE plotter.cpp)
endif()

# Find ALSA
#find_package(ALSA)
if (ALSA_FOUND)
    target_sources(suo PRIVATE signal-io/alsa_io.cpp)
    target_include_directories(suo PRIVATE ${ALSA_INCLUDE_DIRS})
    target_link_libraries(suo PUBLIC ${ALSA_LIBRARIES})
    target_compile_definitions(suo SUO_SUPPORT_ALSA)
endif()


install( TARGETS suo LIBRARY DESTINATION lib )
#install( FILES suo.hpp DESTINATION include )
install( DIRECTORY . DESTINATION include/suo FILES_MATCHING PATTERN "*.hpp" )
