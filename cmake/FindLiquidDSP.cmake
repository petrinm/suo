# - Find LIQUIDDSP
# Find the native LIQUID-DSP includes and library
#
#  LIQUIDDSP_INCLUDES    - where to find LIQUID.h
#  LIQUIDDSP_LIBRARIES   - List of libraries when using LIQUID.
#  LIQUIDDSP_FOUND       - True if LIQUID found.

if (LIQUIDDSP_INCLUDES)
  # Already in cache, be silent
  set (LIQUIDDSP_FIND_QUIETLY TRUE)
endif (LIQUIDDSP_INCLUDES)

find_path (LIQUIDDSP_INCLUDES liquid/liquid.h)

find_library (LIQUIDDSP_LIBRARIES NAMES liquid)

# handle the QUIETLY and REQUIRED arguments and set LIQUID_FOUND to TRUE if
# all listed variables are TRUE
include (FindPackageHandleStandardArgs)
find_package_handle_standard_args (LIQUIDDSP DEFAULT_MSG LIQUIDDSP_LIBRARIES LIQUIDDSP_INCLUDES)

#mark_as_advanced (LIQUIDDSP_LIBRARIES LIQUID_INCLUDES)
