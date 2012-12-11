cmake_minimum_required(VERSION 2.8)
project(metadata-protobuf-v0-plugin)

macro(show v)
  message("${v} is ${${v}}")
endmacro()

################################################################################
# CONFIG
################################################################################
set(PBUFV0_TEST_DATA_PATH ${PROJECT_SOURCE_DIR}/test/data)
configure_file(${PROJECT_SOURCE_DIR}/config.h.in ${PROJECT_BINARY_DIR}/config.h)
include_directories(${PROJECT_BINARY_DIR})

find_package(PROTOBUF CONFIG PATHS cmake)
find_package(EIGEN CONFIG PATHS cmake)
if(PROTOBUF_FOUND AND EIGEN_FOUND)
  include_directories(${PROTOBUF_INCLUDE_DIR})
  include_directories(${EIGEN_INCLUDE_DIR})
  get_directory_property(TILEBASE_SOURCE_DIR PARENT_DIRECTORY)
  include_directories(${TILEBASE_SOURCE_DIR})

  ##############################################################################
  # SOURCE
  ##############################################################################

  file(GLOB SRCS src/*.c src/*.cc)
  file(GLOB HDRS src/*.h src/*.hh)

  set(PROTO_DIR ${PROJECT_SOURCE_DIR}/protos)
  file(GLOB PROTOS ${PROTO_DIR}/*.proto)
  PROTOBUF_GENERATE_CPP(PROTO_SRCS PROTO_HDRS ${PROTO_DIR} ${PROTOS})
  source_group("Header Files\\Protobuf Generated" FILES ${PROTO_HDRS})
  source_group("Source Files\\Protobuf Generated" FILES ${PROTO_SRCS})
  source_group("Protobuf Files" FILES ${PROTOS})

  set(CFG ${PROJECT_BINARY_DIR}/config.h config.h.in)

  set(ALL ${SRCS} ${HDRS} ${PROTOS} ${PROTO_SRCS} ${PROTO_HDRS})
  

  ##############################################################################
  # TARGETS
  ##############################################################################

  add_definitions(-fPIC)
  add_library(meta-protobuf-v0 MODULE ${ALL})
  add_dependencies(meta-protobuf-v0 eigen)
  target_link_libraries(meta-protobuf-v0
    tilebase
    ${PROTOBUF_LIBRARY})


  ##############################################################################
  #  Testing
  ##############################################################################


  find_package(GTEST CONFIG PATHS cmake)
  file(GLOB TEST_SOURCES test/*.cc)
  if(GTEST_FOUND AND TEST_SOURCES)
    find_package(Threads)
    enable_testing()
    include_directories(${PROJECT_SOURCE_DIR})
    include_directories(${GTEST_INCLUDE_DIR})
    add_executable(test-meta-protobuf-v0 ${TEST_SOURCES} ${CFG})
    target_link_libraries(test-meta-protobuf-v0
      ${GTEST_BOTH_LIBRARIES}
      ${CMAKE_THREAD_LIBS_INIT}
      tilebase
      ${PROTOBUF_LIBRARY}
      )
    add_dependencies(test-meta-protobuf-v0 gtest)
    gtest_copy_shared_libraries(test-meta-protobuf-v0)
    add_test(TestMetadataProtobufV0 test-meta-protobuf-v0)
    install(TARGETS test-meta-protobuf-v0
          RUNTIME DESTINATION bin)
  endif()

  ##############################################################################
  #  Install
  ##############################################################################
  install(TARGETS meta-protobuf-v0
    EXPORT meta-protobuf-v0-targets
    LIBRARY DESTINATION bin/${TILEBASE_PLUGIN_PATH}
    RUNTIME DESTINATION bin/${TILEBASE_PLUGIN_PATH}
    ARCHIVE DESTINATION lib/${TILEBASE_PLUGIN_PATH}
  )
  export(TARGETS meta-protobuf-v0 FILE meta-protobuf-v0-targets.cmake)
  install(EXPORT meta-protobuf-v0-targets DESTINATION cmake)

endif()

################################################################################
#  Documentation
################################################################################

find_package(Doxygen)
if(DOXYGEN_FOUND)
  configure_file(${CMAKE_CURRENT_SOURCE_DIR}/doc/Doxyfile.in
                 ${CMAKE_CURRENT_BINARY_DIR}/doc/Doxyfile
                 @ONLY
                 )
  add_custom_target(meta-protobuf-v0-docs
    ${DOXYGEN_EXECUTABLE} ${PROJECT_BINARY_DIR}/doc/Doxyfile
    WORKING_DIRECTORY ${PROJECT_BINARY_DIR}
    COMMENT           "Generating API Docs"
    VERBATIM
    )
endif()