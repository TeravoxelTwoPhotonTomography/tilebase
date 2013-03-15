# LIBND PLUGIN
#

include(ExternalProject)
include(FindPackageHandleStandardArgs)

# SANITIZE LISTS
# allows us to pass the list in through the cmake command line
string(REPLACE ";" "^^" SEP_ND_LIBRARIES "${ND_LIBRARIES}")  
string(REPLACE ";" "^^" SEP_NDIO_PLUGIN_PATH "${NDIO_LIBRARIES}")
string(REPLACE ";" "^^" SEP_GTEST_BOTH_LIBRARIES "${GTEST_BOTH_LIBRARIES}")

set(NDIO_FFMPEG_GIT_REPOSITORY ssh://git@bitbucket.org/nclack/ndio-ffmpeg.git CACHE STRING "Location of the git repository for ndio-ffmpeg.")
ExternalProject_Add(ndio-ffmpeg-plugin
  GIT_REPOSITORY ${NDIO_FFMPEG_GIT_REPOSITORY}
  LIST_SEPARATOR ^^
  CMAKE_ARGS -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
             -DCMAKE_INSTALL_PREFIX:PATH=<INSTALL_DIR>
             -DND_LIBRARIES:STRING=${SEP_ND_LIBRARIES}
             -DND_INCLUDE_DIRS:STRING=${ND_INCLUDE_DIRS}
             -DNDIO_PLUGIN_PATH:STRING=${INSTALL_DIR}/bin/plugins^^${NDIO_PLUGIN_PATH}
             -DGTEST_INCLUDE_DIR:PATH=${GTEST_INCLUDE_DIR}
             -DGTEST_BOTH_LIBRARIES:STRING=${SEP_GTEST_BOTH_LIBRARIES}
  )

add_library(ndio-ffmpeg MODULE IMPORTED)
set_target_properties(${plugin} PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES "C"
  IMPORTED_LOCATION "${ND_ROOT_DIR}/bin/plugins/${CMAKE_SHARED_MODULE_PREFIX}${plugin}${CMAKE_SHARED_MODULE_SUFFIX}"
)
add_dependencies(ndio-ffmpeg libnd)

### install plugins to parent project plugins
get_target_property(_path ndio-ffmpeg IMPORTED_LOCATION)
install(FILES ${_path} DESTINATION bin/plugins)

### exit variables
set(ND_ndio-ffmpeg_FOUND TRUE)
