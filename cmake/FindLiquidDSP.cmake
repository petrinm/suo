# - Find LiquidDSP
# Find the native LIQUID-DSP includes and library
#
#  LiquidDSP_INCLUDE_DIRS - where to find liquid/liquid.h
#  LiquidDSP_LIBRARIES   - List of libraries when using liquid.
#  LiquidDSP_FOUND       - True if LIQUID found.

if (LiquidDSP_INCLUDE_DIRS)
  # Already in cache, be silent
  set (LiquidDSP_FIND_QUIETLY TRUE)
endif (LiquidDSP_INCLUDE_DIRS)

find_path (LiquidDSP_INCLUDE_DIRS liquid/liquid.h)

find_library (LiquidDSP_LIBRARIES NAMES liquid)


# Find out LiquidDSP version number
file(READ "${LiquidDSP_INCLUDE_DIRS}/liquid/liquid.h" LiquidDSP_header)
string(REGEX MATCH "LIQUID_VERSION_NUMBER   ([0-9]*)" _ ${LiquidDSP_header})
set(LiquidDSP_VERSION ${CMAKE_MATCH_1})
message("Liquid Version " ${LiquidDSP_VERSION})
unset(LiquidDSP_header)

#if (LiquidDSP_VERSION LESS 1004000)
#    message(FATAL_ERROR "LiquidDSP version 1.4 or newer required!")
#endif()



# handle the QUIETLY and REQUIRED arguments and set LIQUID_FOUND to TRUE if
# all listed variables are TRUE
include (FindPackageHandleStandardArgs)
find_package_handle_standard_args (LiquidDSP DEFAULT_MSG LiquidDSP_LIBRARIES LiquidDSP_INCLUDE_DIRS)

mark_as_advanced (LiquidDSP_LIBRARIES LiquidDSP_INCLUDE_DIRS)
