#
# YAML
#

include(ExternalProject)
include(FindPackageHandleStandardArgs)

set(YAML_URL http://yaml-cpp.googlecode.com/files/yaml-cpp-0.3.0.tar.gz CACHE STRING "Location of the YAML source code download.")
set(YAML_MD5 9aa519205a543f9372bf4179071c8ac6 CACHE STRING "MD5 checksum of the yaml source code archive.")
ExternalProject_Add(yaml
  URL ${YAML_URL}
  URL_MD5 ${YAML_MD5}
  CMAKE_ARGS -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
             -DCMAKE_INSTALL_PREFIX:PATH=<INSTALL_DIR>
  )
get_target_property(YAML_SOURCE_DIR yaml _EP_SOURCE_DIR)
get_target_property(YAML_ROOT_DIR   yaml _EP_INSTALL_DIR)

set(YAML_INCLUDE_DIR ${YAML_ROOT_DIR}/include CACHE PATH "Path to yaml-cpp/yaml.h")


####
#### Refer to yaml's CMakeLists.txt for more information.
####

option(YAML_MSVC_SHARED_RT "MSVC: Build with shared runtime libs (/MD)" ON)
option(YAML_MSVC_STHREADED_RT "MSVC: Build with single-threaded static runtime libs (/ML until VS .NET 2003)" OFF)

# Microsoft VisualC++ specialities
set(LIBRARY_PREFIX ${CMAKE_STATIC_LIBRARY_PREFIX})
if(MSVC)
  set(LIB_RT_SUFFIX "md")
  if(NOT YAML_MSVC_SHARED_RT)  # User wants to have static runtime libraries (/MT, /ML)
    if(YAML_MSVC_STHREADED_RT) # User wants to have old single-threaded static runtime libraries
      set(LIB_RT_SUFFIX "ml")
    else()
      set(LIB_RT_SUFFIX "mt")
    endif()
  endif()
  #
  set(LABEL_SUFFIX "${LABEL_SUFFIX} ${LIB_RT_SUFFIX}")

  # b) Change prefix for static libraries
  set(LIBRARY_PREFIX "lib")
  if(NOT BUILD_SHARED_LIBS)
    set(LIB_TARGET_SUFFIX "${LIB_RT_SUFFIX}")
  endif()
endif()


####
####
####

add_library(libyaml STATIC IMPORTED)
set_target_properties(libyaml PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES "C"
  IMPORTED_LOCATION_DEBUG          "${YAML_ROOT_DIR}/lib/${LIBRARY_PREFIX}yaml-cpp${LIB_TARGET_SUFFIX}d${CMAKE_STATIC_LIBRARY_SUFFIX}"
  IMPORTED_LOCATION_RELEASE        "${YAML_ROOT_DIR}/lib/${LIBRARY_PREFIX}yaml-cpp${LIB_TARGET_SUFFIX}${CMAKE_STATIC_LIBRARY_SUFFIX}"
  IMPORTED_LOCATION_MINSIZEREL     "${YAML_ROOT_DIR}/lib/${LIBRARY_PREFIX}yaml-cpp${LIB_TARGET_SUFFIX}${CMAKE_STATIC_LIBRARY_SUFFIX}"
  IMPORTED_LOCATION_RELWITHDEBINFO "${YAML_ROOT_DIR}/lib/${LIBRARY_PREFIX}yaml-cpp${LIB_TARGET_SUFFIX}${CMAKE_STATIC_LIBRARY_SUFFIX}"
  )
add_dependencies(libyaml yaml)

set(YAML_INCLUDE_DIRS ${YAML_INCLUDE_DIR})
SET(YAML_LIBRARY libyaml)
set(YAML_LIBRARIES ${YAML_LIBRARY})

find_package_handle_standard_args(YAML DEFAULT_MSG
  YAML_LIBRARY
  YAML_INCLUDE_DIR
)