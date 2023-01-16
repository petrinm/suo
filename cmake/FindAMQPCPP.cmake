# - Find AMQPCPP
# Find the native AMQPCPP includes and library
#
#  AMQPCPP_INCLUDE_DIRS - where to find amqpcpp.h
#  AMQPCPP_LIBRARIES    - List of libraries when using amqpcpp.
#  AMQPCPP_FOUND        - True if AMQPCPP found.

if (AMQPCPP_INCLUDE_DIRS)
    # Already in cache, be silent
    set (AMQPCPP_FIND_QUIETLY TRUE)
endif (AMQPCPP_INCLUDE_DIRS)

find_path (AMQPCPP_INCLUDE_DIRS amqpcpp.h)
find_library (AMQPCPP_LIBRARIES NAMES amqpcpp)


# handle the QUIETLY and REQUIRED arguments and set AMQPCPP_FOUND to TRUE if
# all listed variables are TRUE
include (FindPackageHandleStandardArgs)
find_package_handle_standard_args (AMQPCPP DEFAULT_MSG AMQPCPP_LIBRARIES AMQPCPP_INCLUDE_DIRS)

mark_as_advanced (AMQPCPP_LIBRARIES AMQPCPP_INCLUDE_DIRS)