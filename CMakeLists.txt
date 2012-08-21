cmake_minimum_required(VERSION 2.8)
project(tilebase)

set(CPACK_PACKAGE_VERSION_MAJOR 0)
set(CPACK_PACAKGE_VERSION_MINOR 0)
set(CPACK_PACKAGE_VERSION_PATCH 0a)
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY
  "Tile Database"
  )

set(CMAKE_MODULE_PATH  ${CMAKE_MODULE_PATH} ${PROJECT_SOURCE_DIR}/cmake)

macro(show v)
  message("${v} is ${${v}}")
endmacro()

################################################################################
# CONFIG
################################################################################

set(TILEBASE_PLUGIN_PATH plugins)

set(TILEBASE_TEST_DATA_PATH ${PROJECT_SOURCE_DIR}/test/data)
configure_file(${PROJECT_SOURCE_DIR}/config.h.in ${PROJECT_BINARY_DIR}/config.h)
include_directories(${PROJECT_BINARY_DIR})

find_package(ND)
include_directories(${ND_INCLUDE_DIR})

################################################################################
# SOURCE
################################################################################

file(GLOB SRCS src/*.c src/metadata/*.c)
file(GLOB HDRS src/*.h src/metadata/*.h src/util/*.h)

################################################################################
# TARGETS
################################################################################

add_definitions(-fPIC)
add_library(tilebase
  ${SRCS} ${HDRS}
  config.h.in
  ${PROJECT_BINARY_DIR}/config.h
  )
target_link_libraries(tilebase 
  ${ND_LIBRARY}
  ${CMAKE_DL_LIBS})

# ndio plugins
file(GLOB PLUGINS plugins/*)
foreach(PLUGIN ${PLUGINS})
  add_subdirectory(${PLUGIN})
endforeach(PLUGIN)

###############################################################################
#  Testing
###############################################################################

include(GenerateGoogleTest)
file(GLOB TEST_SOURCES test/*.cc)
if(GTEST_FOUND AND TEST_SOURCES)
  find_package(Threads)
  enable_testing()
  include_directories(${PROJECT_SOURCE_DIR})
  include_directories(${GTEST_INCLUDE_DIR})
  
  add_executable(tilebase-tests ${TEST_SOURCES})
  target_link_libraries(tilebase-tests
    ${GTEST_BOTH_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
    tilebase
    )
  add_dependencies(tilebase-tests
    gtest
    ${PLUGINS}
  )
  add_test(TestTilebase tilebase-tests)
  install(TARGETS tilebase-tests
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
  RUNTIME DESTINATION bin/
  LIBRARY DESTINATION lib/
  ARCHIVE DESTINATION lib/static
  PUBLIC_HEADER DESTINATION include/tilebase
)
export(TARGETS tilebase FILE tilebase-targets.cmake)
install(EXPORT tilebase-targets DESTINATION cmake/)


