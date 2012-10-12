cmake_minimum_required(VERSION 2.8)
project(tilebase)

set(CPACK_PACKAGE_VERSION_MAJOR 0)
set(CPACK_PACAKGE_VERSION_MINOR 0)
set(CPACK_PACKAGE_VERSION_PATCH 0a)
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY
  "Tile Database"
  )

macro(show v)
  message("${v} is ${${v}}")
endmacro()

################################################################################
# CONFIG
################################################################################

set(TILEBASE_PLUGIN_PATH plugins)

find_package(YAML CONFIG PATHS cmake)
include_directories(${YAML_INCLUDE_DIRS})

find_package(ND CONFIG PATHS cmake)
include_directories(${ND_INCLUDE_DIRS})

set(TILEBASE_TEST_DATA_PATH ${PROJECT_SOURCE_DIR}/test/data)
configure_file(${PROJECT_SOURCE_DIR}/config.h.in ${PROJECT_BINARY_DIR}/config.h)
include_directories(${PROJECT_BINARY_DIR})

### Windows shell lightweight utility functions - for plugin search
if(WIN32)
find_library(SHLWAPI Shlwapi.lib) 
else()
set(SHLWAPI)
endif()

################################################################################
# SOURCE
################################################################################

include_directories(${PROJECT_SOURCE_DIR})
file(GLOB SRCS src/*.c src/*.cc src/metadata/*.c)
file(GLOB HDRS src/*.h src/metadata/*.h src/util/*.h)

################################################################################
# TARGETS
################################################################################

### LIBRARY ###

add_definitions(-fPIC)
add_library(tilebase STATIC
  ${SRCS} ${HDRS}
  config.h.in
  ${PROJECT_BINARY_DIR}/config.h
  )
target_link_libraries(tilebase 
  ${ND_LIBRARIES}
  ${CMAKE_DL_LIBS}
  ${SHLWAPI}
  ${YAML_LIBRARIES}
)
# tile metadata plugins
file(GLOB PLUGINS plugins/*)
foreach(PLUGIN ${PLUGINS})
  add_subdirectory(${PLUGIN})
endforeach(PLUGIN)

### macro for copying plugins to build products
macro(tilebase_copy_plugins_to_target _target)
  foreach(PLUGIN ${PLUGINS})
    get_filename_component(_plugin_tgt ${PLUGIN} NAME)
    get_target_property(loc ${_plugin_tgt} LOCATION)
    add_custom_command(TARGET ${_target} POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:${_target}>/plugins
      COMMAND ${CMAKE_COMMAND} -E copy
          $<TARGET_FILE:${_plugin_tgt}>
          $<TARGET_FILE_DIR:${_target}>/plugins
          COMMENT "Copying ${_plugin_tgt} to build path for ${_target}."
          )  
    add_dependencies(${_target} v)
  endforeach()
endmacro()

### APPS ###
add_subdirectory(app/export-quadtree)
add_subdirectory(app/tilebase-cache-build)

###############################################################################
#  Testing
###############################################################################

find_package(GTEST CONFIG PATHS cmake)
file(GLOB TEST_SOURCES test/*.cc)
if(GTEST_FOUND AND TEST_SOURCES)
  find_package(Threads)
  enable_testing()
  include_directories(${PROJECT_SOURCE_DIR})
  include_directories(${GTEST_INCLUDE_DIR})
  
  add_executable(test-tilebase ${TEST_SOURCES})

  target_link_libraries(test-tilebase    
    ${GTEST_BOTH_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
    tilebase
    )
  add_dependencies(test-tilebase
    gtest
    ${PLUGINS}
  )
  nd_copy_plugins_to_target(test-tilebase)
  add_test(TestTilebase test-tilebase)
  install(TARGETS test-tilebase
          RUNTIME DESTINATION bin)
endif()

###############################################################################
#  Documentation
###############################################################################

find_package(Doxygen)
if(DOXYGEN_FOUND)
  configure_file(${CMAKE_CURRENT_SOURCE_DIR}/doc/Doxyfile.in
                 ${CMAKE_CURRENT_BINARY_DIR}/doc/Doxyfile
                 @ONLY
                 )
  add_custom_target(tilebase-docs
    ${DOXYGEN_EXECUTABLE} ${PROJECT_BINARY_DIR}/doc/Doxyfile
    WORKING_DIRECTORY ${PROJECT_BINARY_DIR}
    COMMENT           "Generating API Docs"
    VERBATIM
    )
endif()

###############################################################################
#  Install
###############################################################################
install(TARGETS tilebase
  EXPORT tilebase-targets
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
  PUBLIC_HEADER DESTINATION include/tilebase
)
export(TARGETS tilebase FILE tilebase-targets.cmake)
install(EXPORT tilebase-targets DESTINATION cmake/)

