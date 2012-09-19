# Download and build the Google Protocol Buffers Library.
#

include(ExternalProject)
include(FindPackageHandleStandardArgs)

#
# === PACKAGE ===
#

if(NOT TARGET protobuf)
  ExternalProject_Add(protobuf
    URL     http://protobuf.googlecode.com/files/protobuf-2.4.1.tar.gz
    URL_MD5 dc84e9912ea768baa1976cb7bbcea7b5
    CONFIGURE_COMMAND <SOURCE_DIR>/configure --prefix=<INSTALL_DIR>
  )
  get_target_property(PROTOBUF_ROOT_DIR protobuf _EP_INSTALL_DIR)
  set(PROTOBUF_INCLUDE_DIR ${PROTOBUF_ROOT_DIR}/include CACHE PATH "Location of google/protobuf/message.h" FORCE)

  add_library(libprotobuf STATIC IMPORTED)
  set_target_properties(libprotobuf PROPERTIES
    IMPORTED_LINK_INTERFACE_LANGUAGES "C"
    IMPORTED_LOCATION "${PROTOBUF_ROOT_DIR}/lib/${CMAKE_STATIC_LIBRARY_PREFIX}protobuf${CMAKE_STATIC_LIBRARY_SUFFIX}"
  )
  add_dependencies(libprotobuf protobuf)

  add_executable(protoc IMPORTED)
  set_target_properties(protoc PROPERTIES
    IMPORTED_LOCATION "${PROTOBUF_ROOT_DIR}/bin/protoc"
  )
  add_dependencies(protoc protobuf)  
endif()

set(PROTOBUF_LIBRARY libprotobuf)
set(PROTOBUF_PROTOC_EXECUTABLE protoc)

find_package_handle_standard_args(PROTOBUF DEFAULT_MSG
  PROTOBUF_LIBRARY
  PROTOBUF_INCLUDE_DIR
  PROTOBUF_PROTOC_EXECUTABLE
)

#
# === FUNCTIONS ===
#

function(PROTOBUF_GENERATE_CPP SRCS HDRS PROTOPATH)
  if(NOT ARGN)
    message(SEND_ERROR "Error: PROTOBUF_GENERATE_CPP() called without any proto files")
    return()
  endif(NOT ARGN)

  set(PPTH --proto_path ${CMAKE_CURRENT_SOURCE_DIR})
  if(PROTOPATH)
    # Just use the last path.
    # I get unexpected behavior with multiple paths
    foreach(P ${PROTOPATH})
      get_filename_component(ABS_P ${P} ABSOLUTE)
      set(PPTH --proto_path ${ABS_P})
    endforeach()
  endif()

  set(${SRCS})
  set(${HDRS})
  foreach(FIL ${ARGN})
    get_filename_component(ABS_FIL ${FIL} ABSOLUTE)
    get_filename_component(FIL_WE ${FIL} NAME_WE)
    
    list(APPEND ${SRCS} "${CMAKE_CURRENT_BINARY_DIR}/${FIL_WE}.pb.cc")
    list(APPEND ${HDRS} "${CMAKE_CURRENT_BINARY_DIR}/${FIL_WE}.pb.h")

      #ARGS --cpp_out  ${CMAKE_CURRENT_BINARY_DIR} --proto_path ${CMAKE_CURRENT_SOURCE_DIR} ${ABS_FIL}
    add_custom_command(
      OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${FIL_WE}.pb.cc"
             "${CMAKE_CURRENT_BINARY_DIR}/${FIL_WE}.pb.h"
      COMMAND  ${PROTOBUF_PROTOC_EXECUTABLE}
      ARGS --cpp_out  ${CMAKE_CURRENT_BINARY_DIR} ${PPTH} ${ABS_FIL}
      DEPENDS ${ABS_FIL}
      COMMENT "Running C++ protocol buffer compiler on ${FIL}"
      VERBATIM )
  endforeach()

  set_source_files_properties(${${SRCS}} ${${HDRS}} PROPERTIES GENERATED TRUE)
  set(${SRCS} ${${SRCS}} PARENT_SCOPE)
  set(${HDRS} ${${HDRS}} PARENT_SCOPE)
endfunction()

function(PROTOBUF_GENERATE_PYTHON SRCS PROTOPATH)
  if(NOT ARGN)
    message(SEND_ERROR "Error: PROTOBUF_GENERATE_PYTHON() called without any proto files")
    return()
  endif(NOT ARGN)

  set(PPTH --proto_path ${CMAKE_CURRENT_SOURCE_DIR})
  if(PROTOPATH)
    foreach(P ${PROTOPATH})
      get_filename_component(ABS_P ${P} ABSOLUTE)
      set(PPTH ${PPTH} --proto_path ${ABS_P})
    endforeach()
  endif()

  set(${SRCS})
  foreach(FIL ${ARGN})
    get_filename_component(ABS_FIL ${FIL} ABSOLUTE)
    get_filename_component(FIL_WE ${FIL} NAME_WE)
    
    list(APPEND ${SRCS} "${CMAKE_CURRENT_BINARY_DIR}/${FIL_WE}_pb2.py")

      #ARGS --cpp_out  ${CMAKE_CURRENT_BINARY_DIR} --proto_path ${CMAKE_CURRENT_SOURCE_DIR} ${ABS_FIL}
    add_custom_command(
      OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${FIL_WE}_pb2.py"
      COMMAND  ${PROTOBUF_PROTOC_EXECUTABLE}
      ARGS --python_out=${CMAKE_CURRENT_BINARY_DIR} ${PPTH} ${ABS_FIL}
      DEPENDS ${ABS_FIL}
      COMMENT "Running Python protocol buffer compiler on ${FIL}"
      VERBATIM )
  endforeach()

  set_source_files_properties(${${SRCS}} ${${HDRS}} PROPERTIES GENERATED TRUE)
  set(${SRCS} ${${SRCS}} PARENT_SCOPE)
endfunction()  