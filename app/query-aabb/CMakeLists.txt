cmake_minimum_required(VERSION 2.8)
project(query-aabb)

file(GLOB SRCS src/*.h src/*.c src/*.cc)

set(_target query-aabb)
add_executable(${_target} query-aabb.c ${SRCS})
target_link_libraries(${_target}
  tilebase
  )
set_target_properties(${_target} PROPERTIES INSTALL_RPATH ${RPATH})
tilebase_copy_plugins_to_target(${_target})
nd_copy_plugins_to_target(${_target} ${ND_PLUGINS})
install(TARGETS ${_target} RUNTIME DESTINATION bin)
