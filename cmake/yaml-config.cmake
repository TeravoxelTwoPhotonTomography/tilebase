#
# YAML
#

include(ExternalProject)
include(FindPackageHandleStandardArgs)

macro(show v)
  message("${v} is ${${v}}")
endmacro()

set(YAML_URL http://pyyaml.org/download/libyaml/yaml-0.1.4.tar.gz CACHE STRING "Location of the YAML source code download.")
set(YAML_MD5 36c852831d02cf90508c29852361d01b CACHE STRING "MD5 checksum of the yaml source code archive.")
ExternalProject_Add(yaml
  URL ${YAML_URL}
  URL_MD5 ${YAML_MD5}
  CONFIGURE_COMMAND <SOURCE_DIR>/configure --prefix=<INSTALL_DIR>
  )
get_target_property(YAML_ROOT_DIR yaml _EP_INSTALL_DIR)

set(YAML_INCLUDE_DIR ${YAML_ROOT_DIR}/include CACHE PATH "Path to yaml.h")

add_library(libyaml STATIC IMPORTED)
set_target_properties(libyaml PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES "C"
  IMPORTED_LOCATION                "${YAML_ROOT_DIR}/lib/${CMAKE_STATIC_LIBRARY_PREFIX}yaml${CMAKE_STATIC_LIBRARY_SUFFIX}"
  IMPORTED_LOCATION_DEBUG          "${YAML_ROOT_DIR}/lib/${CMAKE_STATIC_LIBRARY_PREFIX}yaml${CMAKE_STATIC_LIBRARY_SUFFIX}"
  IMPORTED_LOCATION_RELEASE        "${YAML_ROOT_DIR}/lib/${CMAKE_STATIC_LIBRARY_PREFIX}yaml${CMAKE_STATIC_LIBRARY_SUFFIX}"
  IMPORTED_LOCATION_MINSIZEREL     "${YAML_ROOT_DIR}/lib/${CMAKE_STATIC_LIBRARY_PREFIX}yaml${CMAKE_STATIC_LIBRARY_SUFFIX}"
  IMPORTED_LOCATION_RELWITHDEBINFO "${YAML_ROOT_DIR}/lib/${CMAKE_STATIC_LIBRARY_PREFIX}yaml${CMAKE_STATIC_LIBRARY_SUFFIX}"
  )
add_dependencies(libyaml yaml)

set(YAML_INCLUDE_DIRS ${YAML_INCLUDE_DIR})
SET(YAML_LIBRARY libyaml)
set(YAML_LIBRARIES ${YAML_LIBRARY})

find_package_handle_standard_args(YAML DEFAULT_MSG
  YAML_LIBRARY
  YAML_INCLUDE_DIR
)