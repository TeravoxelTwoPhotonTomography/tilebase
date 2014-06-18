cmake_minimum_required(VERSION 2.8)
project(tilebase-cache-build)

configure_file(${PROJECT_SOURCE_DIR}/config.h.in ${PROJECT_BINARY_DIR}/config.h)
include_directories(${PROJECT_BINARY_DIR})

set(_target tilebase-cache-build)
add_executable(${_target} tilebase-cache-build.c)
target_link_libraries(${_target}
  tilebase
  )
set_target_properties(${_target} PROPERTIES INSTALL_RPATH ${RPATH})
tilebase_copy_plugins_to_target(${_target})
nd_copy_plugins_to_target(${_target} ${ND_PLUGINS})
install(TARGETS ${_target} RUNTIME DESTINATION bin)

install(DIRECTORY util/ DESTINATION scripts/tilebase-cache-build 
        USE_SOURCE_PERMISSIONS)
