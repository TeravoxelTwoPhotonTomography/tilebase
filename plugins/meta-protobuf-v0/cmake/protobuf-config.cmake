# Download and build the Google Protocol Buffers Library.
#
include(ExternalProject)
include(FindPackageHandleStandardArgs)

#
# === PACKAGE ===
#

set(PROTOBUF_URL http://dl.dropbox.com/u/782372/Software/protobuf-2.4.1.zip) 
set(PROTOBUF_MD5 ce3ef48c322ea14016fdf845219f386a)

if(WIN32)
  if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(PLAT x64)
  else()
    set(PLAT x86)
  endif()
  # Ideally something like this would work for 64-biut builds: devenv /Command "Build.SolutionPlatforms x64" /Upgrade /SafeMode
  # The shipped protobuf.sln only has the Win32 configuration built in.
  # Unfortunantly, you have to make devenv.exe LARGEADDRESSAWARE in order to get the command to run.
  # So leaving this unfixed for now.  You should manually fixup the protobuf.sln file if necessary.
  if(NOT TARGET protobuf)
    ExternalProject_Add(protobuf
      URL     ${PROTOBUF_URL}
      URL_MD5 ${PROTOBUF_MD5}
      BUILD_IN_SOURCE 1
      LIST_SEPARATOR ^^
      CONFIGURE_COMMAND ""#devenv /Upgrade /SafeMode /Command "Build.SolutionPlatforms x64" <SOURCE_DIR>/vsprojects/protobuf.sln
      BUILD_COMMAND msbuild /target:protoc /target:libprotobuf /p:Configuration=Debug /p:Platform=${PLAT} <SOURCE_DIR>/vsprojects/protobuf.sln
      INSTALL_COMMAND ""
    )
  endif()
  set(PROP _EP_SOURCE_DIR)
  set(INC src)
  set(LIB vsprojects/Debug) #${CMAKE_CFG_INTDIR})
  set(BIN vsprojects/Debug) #${CMAKE_CFG_INTDIR})
else()
  if(NOT TARGET protobuf)
    ExternalProject_Add(protobuf
      URL     ${PROTOBUF_URL}
      URL_MD5 ${PROTOBUF_MD5}
      CONFIGURE_COMMAND <SOURCE_DIR>/configure;--prefix=<INSTALL_DIR>;--with-pic
    )
  endif()
  set(PROP _EP_INSTALL_DIR)
  set(INC include)
  set(LIB lib)
  set(BIN bin)  
endif()
get_target_property(PROTOBUF_ROOT_DIR protobuf ${PROP})

set(PROTOBUF_INCLUDE_DIR ${PROTOBUF_ROOT_DIR}/${INC} CACHE PATH "Location of google/protobuf/message.h" FORCE)

add_library(libprotobuf STATIC IMPORTED)
set_target_properties(libprotobuf PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES "C"
  IMPORTED_LOCATION "${PROTOBUF_ROOT_DIR}/${LIB}/libprotobuf${CMAKE_STATIC_LIBRARY_SUFFIX}"
)
add_dependencies(libprotobuf protobuf)

add_executable(protoc IMPORTED)
set_target_properties(protoc PROPERTIES
  IMPORTED_LOCATION "${PROTOBUF_ROOT_DIR}/${BIN}/protoc"
)
add_dependencies(protoc protobuf)  


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
