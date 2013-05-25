cmake_minimum_required(VERSION 2.8)
project(query-name)

find_package(TRE CONFIG PATHS cmake)
include_directories(${TRE_INCLUDE_DIR})

file(GLOB SRCS src/*.h src/*.c src/*.cc)

set(_target query-name)
add_executable(${_target} query-name.c ${SRCS})
target_link_libraries(${_target}
  tilebase
  ${TRE_LIBRARIES}
  )
set_target_properties(${_target} PROPERTIES INSTALL_RPATH ${RPATH})
tilebase_copy_plugins_to_target(${_target})
nd_copy_plugins_to_target(${_target} ${ND_PLUGINS})
install(TARGETS ${_target} RUNTIME DESTINATION bin)
