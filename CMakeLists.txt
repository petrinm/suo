project(suo)
cmake_minimum_required(VERSION 3.16)


include(CMakeFindDependencyMacro)
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)


set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 10)
    message( FATAL_ERROR "ERROR GCC versions older than GCC10 are not supported.")
endif()

if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 11)
    # On GCC 10 extra '-fcoroutine' flag is required to enable C++ coroutines.
    add_compile_options(-fcoroutine)
endif()


#enable_testing()
#set(CMAKE_BUILD_TYPE Debug) # Force debug mode

add_compile_options(-g -Wall -Wextra)
add_compile_options(-Wno-unused)

add_subdirectory(libsuo)
#add_subdirectory(apps)
add_subdirectory(tests)
