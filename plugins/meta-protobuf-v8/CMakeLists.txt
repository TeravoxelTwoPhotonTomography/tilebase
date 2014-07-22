cmake_minimum_required(VERSION 2.8)
project(metadata-protobuf-v8-plugin)

macro(show v)
  message("${v} is ${${v}}")
endmacro()

################################################################################
# CONFIG
################################################################################
set(PBUFV8_TEST_DATA_PATH ${PROJECT_SOURCE_DIR}/test/data)
configure_file(${PROJECT_SOURCE_DIR}/config.h.in ${PROJECT_BINARY_DIR}/config.h)
include_directories(${PROJECT_BINARY_DIR})

find_package(PROTOBUF CONFIG PATHS cmake)
find_package(EIGEN CONFIG PATHS cmake)
find_package(Threads)
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

  set(CFG ${PROJECT_BINARY_DIR}/config.h ${PROJECT_SOURCE_DIR}/config.h.in)

  set(ALL ${SRCS} ${HDRS} ${PROTOS} ${PROTO_SRCS} ${PROTO_HDRS})

  ##############################################################################
  # TARGETS
  ##############################################################################

  add_library(meta-protobuf-v8 MODULE ${ALL})
  add_dependencies(meta-protobuf-v8 eigen)
  target_link_libraries(meta-protobuf-v8
    tilebase
    ${CMAKE_THREAD_LIBS_INIT}
    ${PROTOBUF_LIBRARY})
  set_target_properties(meta-protobuf-v8 PROPERTIES POSITION_INDEPENDENT_CODE TRUE)

  ##############################################################################
  #  Testing
  ##############################################################################
  add_subdirectory(test)

  ##############################################################################
  #  Install
  ##############################################################################
  install(TARGETS meta-protobuf-v8
    EXPORT meta-protobuf-v8-targets
    LIBRARY DESTINATION bin/${TILEBASE_PLUGIN_PATH}
    RUNTIME DESTINATION bin/${TILEBASE_PLUGIN_PATH}
    ARCHIVE DESTINATION lib/${TILEBASE_PLUGIN_PATH}
  )
  export(TARGETS meta-protobuf-v8 FILE meta-protobuf-v8-targets.cmake)
  install(EXPORT meta-protobuf-v8-targets DESTINATION cmake)

endif()

################################################################################
#  Documentation
################################################################################
#add_subdirectory(doc)
