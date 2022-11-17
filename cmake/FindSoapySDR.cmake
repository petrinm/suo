# - Find SoapySDR
# Find the native SoapySDR includes and library
#
#  SoapySDR_INCLUDES    - where to find SoapySDR/Device.hpp
#  SoapySDR_LIBRARIES   - List of libraries when using SoapySDR.
#  SoapySDR_FOUND       - True if SoapySDR found.

if (SoapySDR_INCLUDES)
  # Already in cache, be silent
  set (SoapySDR_FIND_QUIETLY TRUE)
endif (SoapySDR_INCLUDES)

find_path (SoapySDR_INCLUDES SoapySDR/Device.hpp)
find_library (SoapySDR_LIBRARIES NAMES SoapySDR)


# handle the QUIETLY and REQUIRED arguments and set SoapySDR_FOUND to TRUE if
# all listed variables are TRUE
include (FindPackageHandleStandardArgs)
find_package_handle_standard_args (SoapySDR DEFAULT_MSG SoapySDR_LIBRARIES SoapySDR_INCLUDES)

#mark_as_advanced (SoapySDR_LIBRARIES SoapySDR_INCLUDES)
