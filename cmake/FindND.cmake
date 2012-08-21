#
# LIBND
#

include(ExternalProject)
include(FindPackageHandleStandardArgs)

set(ND_GIT_REPOSITORY ssh://git@bitbucket.org/nclack/nd.git CACHE STRING "Location of the git repository for libnd.")

ExternalProject_Add(libnd
  GIT_REPOSITORY ${ND_GIT_REPOSITORY}
  # UPDATE_COMMAND ""
  UPDATE_COMMAND ${GIT_EXECUTABLE} pull origin master
  CMAKE_ARGS -DCMAKE_INSTALL_PREFIX:PATH=<INSTALL_DIR>
             -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
  )
get_target_property(_ND_INCLUDE_DIR libnd _EP_SOURCE_DIR)
get_target_property(ND_ROOT_DIR     libnd _EP_INSTALL_DIR)

set(ND_INCLUDE_DIR ${_ND_INCLUDE_DIR} CACHE PATH "Path to nd.h")

add_library(nd STATIC IMPORTED)
set_target_properties(nd PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES "C"
  IMPORTED_LOCATION "${ND_ROOT_DIR}/lib/static/${CMAKE_STATIC_LIBRARY_PREFIX}nd${CMAKE_STATIC_LIBRARY_SUFFIX}"
  )
add_dependencies(nd libnd)

foreach(plugin ndio-ffmpeg ndio-tiff)
  add_library(${plugin} MODULE IMPORTED)
  set_target_properties(${plugin} PROPERTIES
    IMPORTED_LINK_INTERFACE_LANGUAGES "C"
    IMPORTED_LOCATION "${ND_ROOT_DIR}/bin/plugins/${CMAKE_SHARED_MODULE_PREFIX}${plugin}{CMAKE_SHARED_MODULE_SUFFIX}"
    )
  add_dependencies(${plugin} libnd)
endforeach()

add_executable(libnd-tests IMPORTED)
set_target_properties(libnd-tests PROPERTIES
  IMPORTED_LOCATION "${ND_ROOT_DIR}/bin/libnd-tests${CMAKE_EXECUTABLE_SUFFIX}"
  )
add_dependencies(libnd-tests libnd)

set(ND_LIBRARY nd)
set(ND_PLUGINS ndio-ffmpeg ndio-tiff)

find_package_handle_standard_args(ND DEFAULT_MSG
  ND_LIBRARY
  ND_INCLUDE_DIR
)
