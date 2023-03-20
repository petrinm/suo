# - Find SoapySDR
# Find the native SoapySDR includes and library
#
#  SoapySDR_INCLUDE_DIRS - where to find SoapySDR/Device.hpp
#  SoapySDR_LIBRARIES   - List of libraries when using SoapySDR.
#  SoapySDR_FOUND       - True if SoapySDR found.

if (SoapySDR_INCLUDE_DIRS)
  # Already in cache, be silent
  set (SoapySDR_FIND_QUIETLY TRUE)
endif (SoapySDR_INCLUDE_DIRS)

find_path (SoapySDR_INCLUDE_DIRS SoapySDR/Device.hpp)
find_library (SoapySDR_LIBRARIES NAMES SoapySDR)


# handle the QUIETLY and REQUIRED arguments and set SoapySDR_FOUND to TRUE if
# all listed variables are TRUE
include (FindPackageHandleStandardArgs)
find_package_handle_standard_args (SoapySDR DEFAULT_MSG SoapySDR_LIBRARIES SoapySDR_INCLUDE_DIRS)

mark_as_advanced (SoapySDR_LIBRARIES SoapySDR_INCLUDE_DIRS)
